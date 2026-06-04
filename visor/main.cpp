// Motor Gráfico — Visor 3D interactivo con workspace
// Capas: os/ + render/ + scene/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <cstdio>
#include <string>
#include "../os/os.h"
#include "../render/scene.h"
#include "../render/render.h"
#include "../scene/scene_graph.h"
#include "../scene/camera.h"
#include "../scene/scene_query.h"
#include "../scene/workspace.h"

using namespace mg;

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------

struct App {
    HWND hwnd = nullptr;
    HWND hwnd_viewport = nullptr;
    HWND hwnd_edit = nullptr;
    HWND hwnd_status = nullptr;

    Renderer renderer;

    // Scene layer
    scene::SceneGraph       graph;
    scene::CameraController cam_ctrl;
    scene::Workspace        workspace;
    scene::Bvh              bvh;

    bool dirty = true;
    bool running = true;
    bool mouse_look = false;
    POINT last_mouse;

    std::string scene_path;
    char edit_buffer[32768] = {};
} g_app;

// ---------------------------------------------------------------------------
// Scene loading
// ---------------------------------------------------------------------------

// Per-node world AABBs for AABB early-out (recomputed after scene load or camera move)
static std::vector<Aabb> g_aabbs;

static void rebuildAabbs() {
    g_aabbs.resize(g_app.graph.nodes.size());
    for (u32 i = 0; i < (u32)g_app.graph.nodes.size(); i++) {
        g_app.graph.computeLocalAabb(i, g_app.renderer.scene);
    }
    g_app.graph.updateWorldTransforms();
    for (u32 i = 0; i < (u32)g_app.graph.nodes.size(); i++) {
        g_aabbs[i] = g_app.graph.getWorldAabb(i);
    }
}

// BVH query callback (called per ray from renderScene)
static u32 bvhQuery(void* ctx, const Vec3& ro, const Vec3& rd, u32* out, u32 max) {
    (void)ctx;
    if (g_app.bvh.nodes.empty()) return 0;
    static std::vector<u32> tmp;
    g_app.bvh.query(ro, rd, tmp);
    u32 n = (u32)tmp.size() < max ? (u32)tmp.size() : max;
    for (u32 i = 0; i < n; i++) out[i] = tmp[i];
    return n;
}

static void loadScene(const wchar_t* path) {
    FileMapping fm;
    if (!fm.open(path)) {
        g_app.renderer.scene.loadDefault();
        return;
    }
    g_app.renderer.load(fm);
    fm.close();

    // Init scene graph
    g_app.graph.init(g_app.renderer.scene);
    g_app.graph.updateWorldTransforms();

    // Compute AABBs + build BVH when there are SDF leaves
    bool has_sdf_leaves = false;
    for (u32 i = 0; i < (u32)g_app.graph.nodes.size(); i++) {
        if (g_app.renderer.scene.nodes[i].type == NodeType::SDF && !g_app.renderer.scene.nodes[i].is_compound_child) {
            has_sdf_leaves = true;
            break;
        }
    }
    rebuildAabbs();
    if (has_sdf_leaves) {
        g_app.bvh.build(g_app.graph, g_app.renderer.scene);
    }

    // Camera controller
    g_app.cam_ctrl.setOrbit(g_app.renderer.scene.camera.target, 5.0f, 0, 0.5f);

    // Workspace
    g_app.workspace.init();
    g_app.workspace.active().w = g_app.renderer.scene.width;
    g_app.workspace.active().h = g_app.renderer.scene.height;

    g_app.dirty = true;
}

static void rebuildScene() {
    g_app.graph.init(g_app.renderer.scene);
    rebuildAabbs();
    bool has_sdf_leaves = false;
    for (u32 i = 0; i < (u32)g_app.graph.nodes.size(); i++) {
        if (g_app.renderer.scene.nodes[i].type == NodeType::SDF && !g_app.renderer.scene.nodes[i].is_compound_child) {
            has_sdf_leaves = true; break;
        }
    }
    if (has_sdf_leaves) g_app.bvh.build(g_app.graph, g_app.renderer.scene);
}

// ---------------------------------------------------------------------------
// Camera controls
// ---------------------------------------------------------------------------

static void updateCamera(float dt) {
    // WASD + QE movement in free fly mode
    bool fwd = (GetAsyncKeyState('W') & 0x8000) != 0;
    bool bwd = (GetAsyncKeyState('S') & 0x8000) != 0;
    bool lft = (GetAsyncKeyState('A') & 0x8000) != 0;
    bool rgt = (GetAsyncKeyState('D') & 0x8000) != 0;
    bool up  = (GetAsyncKeyState('Q') & 0x8000) != 0;
    bool dwn = (GetAsyncKeyState('E') & 0x8000) != 0;

    g_app.cam_ctrl.updateFly(dt, fwd, bwd, lft, rgt, up, dwn);

    // Apply to render camera
    g_app.cam_ctrl.applyTo(g_app.renderer.scene.camera);
}

// ---------------------------------------------------------------------------
// Viewport rendering
// ---------------------------------------------------------------------------

static void renderFrame() {
    int w = g_app.workspace.active().w;
    int h = g_app.workspace.active().h;
    if (!g_aabbs.empty() && !g_app.bvh.nodes.empty()) {
        SceneQuery sq;
        sq.aabbs = g_aabbs.data();
        sq.query_fn = bvhQuery;
        g_app.renderer.render(w, h, sq);
    } else if (!g_aabbs.empty()) {
        g_app.renderer.render(w, h, g_aabbs.data(), nullptr, 0);
    } else {
        g_app.renderer.render(w, h);
    }
}

static void paintViewport(HDC hdc) {
    if (!g_app.renderer.fb.pixels) {
        RECT rc;
        GetClientRect(g_app.hwnd_viewport, &rc);
        Vec3 bg = g_app.renderer.scene.background;
        HBRUSH brush = CreateSolidBrush(RGB(
            (int)(bg.x * 255), (int)(bg.y * 255), (int)(bg.z * 255)));
        FillRect(hdc, &rc, brush);
        DeleteObject(brush);
        return;
    }

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = g_app.renderer.fb.width;
    bmi.bmiHeader.biHeight = -(int)g_app.renderer.fb.height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    SetDIBitsToDevice(hdc, 0, 0, g_app.renderer.fb.width, g_app.renderer.fb.height,
                      0, 0, 0, g_app.renderer.fb.height,
                      g_app.renderer.fb.pixels, &bmi, DIB_RGB_COLORS);
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_DESTROY:
            g_app.running = false;
            PostQuitMessage(0);
            return 0;

        case WM_SIZE: {
            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
            int vp_w = w - 300;
            if (vp_w < 100) vp_w = 100;
            if (g_app.hwnd_viewport)
                SetWindowPos(g_app.hwnd_viewport, nullptr, 0, 0, vp_w, h, SWP_NOMOVE | SWP_NOZORDER);
            if (g_app.hwnd_edit)
                SetWindowPos(g_app.hwnd_edit, nullptr, vp_w, 0, w - vp_w, h - 20, SWP_NOZORDER);
            if (g_app.hwnd_status)
                SetWindowPos(g_app.hwnd_status, nullptr, vp_w, h - 20, w - vp_w, 20, SWP_NOZORDER);
            g_app.workspace.active().w = vp_w;
            g_app.workspace.active().h = h;
            g_app.dirty = g_app.hwnd_viewport != nullptr;
            return 0;
        }

        case WM_KEYDOWN:
            if (wp == VK_F5) {
                g_app.dirty = true;
            }
            if (wp == VK_F1) {
                g_app.mouse_look = !g_app.mouse_look;
                if (g_app.mouse_look) {
                    GetCursorPos(&g_app.last_mouse);
                    SetCapture(g_app.hwnd_viewport);
                    ShowCursor(FALSE);
                } else {
                    ReleaseCapture();
                    ShowCursor(TRUE);
                    SetCursorPos(g_app.last_mouse.x, g_app.last_mouse.y);
                }
            }
            if (wp == VK_OEM_PLUS || wp == VK_ADD) {
                g_app.cam_ctrl.zoom(-0.5f);
                g_app.cam_ctrl.applyTo(g_app.renderer.scene.camera);
                g_app.dirty = true;
            }
            if (wp == VK_OEM_MINUS || wp == VK_SUBTRACT) {
                g_app.cam_ctrl.zoom(0.5f);
                g_app.cam_ctrl.applyTo(g_app.renderer.scene.camera);
                g_app.dirty = true;
            }
            return 0;

        case WM_MOUSEMOVE:
            if (g_app.mouse_look && g_app.hwnd_viewport == GetCapture()) {
                int dx = GET_X_LPARAM(lp) - g_app.workspace.active().w / 2;
                int dy = GET_Y_LPARAM(lp) - g_app.workspace.active().h / 2;
                RECT rc;
                GetClientRect(g_app.hwnd_viewport, &rc);
                SetCursorPos(rc.left + g_app.workspace.active().w / 2,
                             rc.top + g_app.workspace.active().h / 2);

                g_app.cam_ctrl.orbitRotate((f32)dx, (f32)(-dy));
                g_app.cam_ctrl.applyTo(g_app.renderer.scene.camera);
                g_app.dirty = true;
            }
            return 0;

        case WM_COMMAND:
            if (HIWORD(wp) == EN_CHANGE && (HWND)lp == g_app.hwnd_edit) {
            }
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Viewport window procedure
// ---------------------------------------------------------------------------

static LRESULT CALLBACK ViewportProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            paintViewport(hdc);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ============================================================================
// WinMain
// ============================================================================

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int) {
    // Resolve .rih path
    wchar_t full_path[MAX_PATH];
    wchar_t exe_dir[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_dir, MAX_PATH);
    wchar_t* p = wcsrchr(exe_dir, L'\\');
    if (p) *p = L'\0';
    wcscpy_s(full_path, exe_dir);
    wcscat_s(full_path, L"\\..\\..\\LENGUA~1\\ejemplos\\bodegon.rih");

    loadScene(full_path);
    if (g_app.renderer.scene.nodes.empty()) {
        g_app.renderer.scene.loadDefault();
        rebuildScene();
    }

    // Default light if none
    if (g_app.renderer.scene.lights.empty()) {
        Light def;
        def.name = "default";
        def.type = LightType::DIRECTIONAL;
        def.direction = {0.3f, 0.8f, 0.5f};
        g_app.renderer.scene.lights.push_back(def);
    }

    // Camera controller
    g_app.cam_ctrl.setOrbit(g_app.renderer.scene.camera.target, 5.0f, 0, 0.5f);
    g_app.cam_ctrl.applyTo(g_app.renderer.scene.camera);

    // Workspace init
    g_app.workspace.init();
    g_app.workspace.active().w = g_app.renderer.scene.width;
    g_app.workspace.active().h = g_app.renderer.scene.height;

    // Register window classes
    const char* CLASS_NAME = "MotorGrafico";
    const char* VIEWPORT_NAME = "MGViewport";
    const char* EDIT_NAME = "MGEdit";

    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    if (!RegisterClass(&wc)) {
        MessageBox(nullptr, "Fallo RegisterClass (main)", "Error", MB_OK);
        return 1;
    }

    WNDCLASS vc = {};
    vc.lpfnWndProc = ViewportProc;
    vc.hInstance = hInst;
    vc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    vc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    vc.lpszClassName = VIEWPORT_NAME;
    if (!RegisterClass(&vc)) {
        MessageBox(nullptr, "Fallo RegisterClass (viewport)", "Error", MB_OK);
        return 1;
    }

    // Create window
    g_app.hwnd = CreateWindowEx(0, CLASS_NAME, "Motor Grafico — Visor 3D",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1100, 700,
        nullptr, nullptr, hInst, nullptr);
    if (!g_app.hwnd) {
        MessageBox(nullptr, "Fallo CreateWindow", "Error", MB_OK);
        return 1;
    }

    // Create child windows
    RECT rc;
    GetClientRect(g_app.hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    int vp_w = w - 300;
    if (vp_w < 100) vp_w = 100;

    g_app.hwnd_viewport = CreateWindowEx(0, VIEWPORT_NAME, nullptr,
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        0, 0, vp_w, h, g_app.hwnd, nullptr, hInst, nullptr);
    g_app.hwnd_edit = CreateWindowEx(0, "EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL |
        WS_VSCROLL | ES_WANTRETURN,
        vp_w, 0, w - vp_w, h - 20, g_app.hwnd, nullptr, hInst, nullptr);
    g_app.hwnd_status = CreateWindowEx(0, "STATIC",
        "F1: mouse look | Mouse drag: orbit | +/-: zoom | F5: render | WASD+QE: free fly",
        WS_CHILD | WS_VISIBLE,
        vp_w, h - 20, w - vp_w, 20, g_app.hwnd, nullptr, hInst, nullptr);

    // Set initial editor text
    std::string edit_text = "// Motor Grafico — Workspace\n"
        "// F1: camara libre | F5: re-render\n"
        "// Mouse drag: orbit | +/-: zoom\n"
        "// WASD+QE: mover (free fly)\n\n"
        "// Escena cargada\n";
    SetWindowText(g_app.hwnd_edit, edit_text.c_str());

    ShowWindow(g_app.hwnd, SW_SHOW);

    // Set title
    char title[512];
    snprintf(title, sizeof(title), "Motor Grafico [%zu nodes, %zu mats, BVH=%zu nodes]",
        g_app.renderer.scene.nodes.size(),
        g_app.renderer.scene.materials.size(),
        g_app.bvh.nodes.size());
    SetWindowText(g_app.hwnd, title);

    // Render resolution
    g_app.renderer.time = 0.0f;
    g_app.dirty = true;

    // Message loop
    MSG msg = {};
    while (g_app.running) {
        while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        updateCamera(0.016f);

        if (g_app.dirty) {
            renderFrame();
            g_app.dirty = false;
            InvalidateRect(g_app.hwnd_viewport, nullptr, FALSE);
            UpdateWindow(g_app.hwnd_viewport);
        }

        Sleep(16);
    }

    return 0;
}
