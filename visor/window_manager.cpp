#include "window_manager.h"
#include "visor_app.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include <cstdio>
#include <string>

// Forward declaration (imgui_impl_win32.h keeps it in a #if 0 block)
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace mg {

// ============================================================================
// GDI Helpers
// ============================================================================
void WindowManager::fillRect(HDC hdc, RECT rc, COLORREF c) {
    HBRUSH brush = CreateSolidBrush(c);
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
}

void WindowManager::drawRect(HDC hdc, RECT rc, COLORREF c) {
    HBRUSH brush = CreateSolidBrush(c);
    FrameRect(hdc, &rc, brush);
    DeleteObject(brush);
}

void WindowManager::drawPanelHeader(HDC hdc, RECT rc, const char* label, COLORREF accent) {
    fillRect(hdc, rc, UIColor::BG_HEADER);
    
    // Bottom border
    RECT rcLine = rc;
    rcLine.top = rc.bottom - 1;
    fillRect(hdc, rcLine, UIColor::BORDER);

    // Accent line on top
    RECT rcAccent = rc;
    rcAccent.bottom = rc.top + 2;
    fillRect(hdc, rcAccent, accent);

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, UIColor::TEXT_PRIMARY);
    rc.left += 10;
    DrawText(hdc, label, -1, &rc, DT_SINGLELINE | DT_VCENTER | DT_LEFT);
}

void WindowManager::drawSeparator(HDC hdc, int x, int y, int w) {
    RECT rc = { x, y, x + w, y + 1 };
    fillRect(hdc, rc, UIColor::BORDER);
}

void WindowManager::drawStatRow(HDC hdc, HFONT font, int x, int& y, const char* key, const char* val, COLORREF val_color) {
    SelectObject(hdc, font);
    SetBkMode(hdc, TRANSPARENT);
    
    RECT rcK = { x, y, x + 100, y + 20 };
    SetTextColor(hdc, UIColor::TEXT_DIM);
    DrawText(hdc, key, -1, &rcK, DT_SINGLELINE | DT_VCENTER | DT_LEFT);

    RECT rcV = { x + 100, y, x + 250, y + 20 };
    SetTextColor(hdc, val_color);
    DrawText(hdc, val, -1, &rcV, DT_SINGLELINE | DT_VCENTER | DT_RIGHT);
    
    y += 24;
}

// ============================================================================
// Resource Management
// ============================================================================
void WindowManager::createGdiResources() {
    brush_main   = CreateSolidBrush(UIColor::BG_MAIN);
    brush_panel  = CreateSolidBrush(UIColor::BG_PANEL);
    brush_edit   = CreateSolidBrush(UIColor::BG_EDIT);
    brush_header = CreateSolidBrush(UIColor::BG_HEADER);

    font_ui = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    
    font_mono = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");

    font_title = CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
}

void WindowManager::destroyGdiResources() {
    if (brush_main) DeleteObject(brush_main);
    if (brush_panel) DeleteObject(brush_panel);
    if (brush_edit) DeleteObject(brush_edit);
    if (brush_header) DeleteObject(brush_header);
    if (font_ui) DeleteObject(font_ui);
    if (font_mono) DeleteObject(font_mono);
    if (font_title) DeleteObject(font_title);
}

// ============================================================================
// Layout
// ============================================================================
void WindowManager::updateLayout() {
    if (!hwnd) return;
    RECT rc;
    GetClientRect(hwnd, &rc);
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    
    if (w < 400 || h < 300) return;

    int vp_w = w - UI_PANEL_W;
    int vp_h = h - UI_TOOLBAR_H - UI_STATUS_H;
    {
        FILE* f = fopen("phase6_diag.txt", "a");
        if (f) { fprintf(f, "[LAYOUT] client=%dx%d -> vp=%dx%d\n", w, h, vp_w, vp_h); fclose(f); }
    }
    int right_x = vp_w;

    if (hwnd_toolbar)
        SetWindowPos(hwnd_toolbar, nullptr, 0, 0, w, UI_TOOLBAR_H, SWP_NOZORDER);
    
    if (hwnd_viewport) {
        SetWindowPos(hwnd_viewport, nullptr, 0, UI_TOOLBAR_H, vp_w, vp_h, SWP_NOZORDER);
        if (app) {
            app->workspace.active().w = vp_w;
            app->workspace.active().h = vp_h;
            app->dirty = true;
        }
    }

    int props_h = vp_h - UI_CONSOLE_H;
    if (hwnd_props)
        SetWindowPos(hwnd_props, nullptr, right_x, UI_TOOLBAR_H, UI_PANEL_W, props_h, SWP_NOZORDER);

    if (hwnd_console)
        SetWindowPos(hwnd_console, nullptr, right_x, UI_TOOLBAR_H + props_h, UI_PANEL_W, UI_CONSOLE_H, SWP_NOZORDER);

    if (hwnd_status)
        SetWindowPos(hwnd_status, nullptr, 0, h - UI_STATUS_H, w, UI_STATUS_H, SWP_NOZORDER);
}

// ============================================================================
// Window Procedures
// ============================================================================
LRESULT CALLBACK WindowManager::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    WindowManager* wm = (WindowManager*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lp;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1; // Handled by WM_PAINT
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (wm) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                fillRect(hdc, rc, UIColor::BG_MAIN);
                
                // Draw borders between panels
                int w = rc.right - rc.left;
                int h = rc.bottom - rc.top;
                int vp_w = w - UI_PANEL_W;
                
                // Vertical divider
                fillRect(hdc, {vp_w - 1, UI_TOOLBAR_H, vp_w, h - UI_STATUS_H}, UIColor::BORDER);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_DESTROY:
            if (wm && wm->app) wm->app->running = false;
            PostQuitMessage(0);
            return 0;

        case WM_SIZE: {
            if (wm) wm->updateLayout();
            return 0;
        }

        case WM_KEYDOWN:
            if (!wm || !wm->app) break;
            if (wp == VK_F5) {
                wm->app->dirty = true;
            }
            if (wm->app->input.handleKeyDown(wp, wm->app->cam_ctrl, wm->hwnd_viewport)) {
                if (wm->app->renderer.ont_mode) {
                    wm->app->cam_ctrl.applyTo(wm->app->renderer.ont_scene.camera);
                } else {
                    wm->app->cam_ctrl.applyTo(wm->app->renderer.scene.camera);
                }
                wm->app->camera_moving = true;
                wm->app->camera_stop_tick = GetTickCount();
                wm->app->dirty = true;
                wm->app->title_dirty = true;
            }
            return 0;

        case WM_MOUSEMOVE:
            if (!wm || !wm->app) break;
            if (wm->app->input.handleMouseMove(lp, wm->app->cam_ctrl, wm->hwnd_viewport, wm->app->workspace.active().w, wm->app->workspace.active().h)) {
                if (wm->app->renderer.ont_mode) {
                    wm->app->cam_ctrl.applyTo(wm->app->renderer.ont_scene.camera);
                } else {
                    wm->app->cam_ctrl.applyTo(wm->app->renderer.scene.camera);
                }
                wm->app->camera_moving = true;
                wm->app->camera_stop_tick = GetTickCount();
                wm->app->dirty = true;
            }
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT CALLBACK WindowManager::ToolbarProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    WindowManager* wm = (WindowManager*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lp;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (wm) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                fillRect(hdc, rc, UIColor::BG_TOOLBAR);
                
                // Bottom border
                RECT rcB = rc;
                rcB.top = rcB.bottom - 1;
                fillRect(hdc, rcB, UIColor::BORDER);

                // Draw some fake tool buttons
                SetBkMode(hdc, TRANSPARENT);
                SelectObject(hdc, wm->font_title);
                
                const char* btns[] = { " Render", " Objects", " Materials", " Settings" };
                int bx = 10;
                for (int i = 0; i < 4; i++) {
                    RECT rb = { bx, 5, bx + 80, 35 };
                    COLORREF bg = (wm->hovered_btn == i) ? UIColor::BTN_HOVER : UIColor::BTN_NORMAL;
                    if (i == 0) bg = UIColor::ACCENT_BLUE; // Highlight first btn
                    
                    fillRect(hdc, rb, bg);
                    drawRect(hdc, rb, UIColor::BORDER_BRIGHT);
                    
                    SetTextColor(hdc, i==0 ? RGB(255,255,255) : UIColor::TEXT_PRIMARY);
                    DrawText(hdc, btns[i], -1, &rb, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
                    bx += 90;
                }
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (!wm) break;
            int x = LOWORD(lp);
            int y = HIWORD(lp);
            int hovered = -1;
            int bx = 10;
            for (int i = 0; i < 4; i++) {
                if (x >= bx && x <= bx + 80 && y >= 5 && y <= 35) {
                    hovered = i; break;
                }
                bx += 90;
            }
            if (hovered != wm->hovered_btn) {
                wm->hovered_btn = hovered;
                InvalidateRect(hwnd, nullptr, FALSE);
                // Track mouse leave
                TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
                TrackMouseEvent(&tme);
            }
            return 0;
        }
        case WM_MOUSELEAVE: {
            if (wm) {
                wm->hovered_btn = -1;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONDOWN: {
            if (wm && wm->hovered_btn == 0 && wm->app) {
                wm->app->dirty = true;
            }
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT CALLBACK WindowManager::PropsProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    WindowManager* wm = (WindowManager*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lp;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return 0;
        }
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (wm) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                fillRect(hdc, rc, UIColor::BG_PANEL);

                // Header
                RECT rcHeader = {0, 0, rc.right, 30};
                drawPanelHeader(hdc, rcHeader, "PROPERTIES", UIColor::ACCENT_ORANGE);

                // Stats
                int y = 45;
                drawStatRow(hdc, wm->font_ui, 15, y, "Engine State", "Running", UIColor::ACCENT_GREEN);
                drawStatRow(hdc, wm->font_ui, 15, y, "Resolution", "Dynamic", UIColor::TEXT_PRIMARY);
                drawStatRow(hdc, wm->font_ui, 15, y, "Renderer", "SDF Raymarch", UIColor::ACCENT_BLUE);
                
                y += 10;
                drawSeparator(hdc, 10, y, rc.right - 20);
                y += 15;

                // Scene Info
                SelectObject(hdc, wm->font_title);
                SetTextColor(hdc, UIColor::TEXT_PRIMARY);
                TextOut(hdc, 15, y, "Scene Info", 10);
                y += 30;

                SelectObject(hdc, wm->font_ui);
                SetTextColor(hdc, UIColor::TEXT_SECONDARY);
                RECT rcText = {15, y, rc.right - 15, rc.bottom - 10};
                DrawText(hdc, wm->props_text.c_str(), -1, &rcText, DT_LEFT | DT_TOP | DT_WORDBREAK);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

LRESULT CALLBACK WindowManager::ViewportProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    WindowManager* wm = (WindowManager*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (ImGui::GetCurrentContext() && ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return 0;
    switch (msg) {
        case WM_CREATE: {
            CREATESTRUCT* cs = (CREATESTRUCT*)lp;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            if (wm && wm->app) {
                wm->paintViewport(hdc, wm->app->renderer.fb.pixels, 
                                  wm->app->renderer.fb.width, wm->app->renderer.fb.height, 
                                  &wm->app->renderer.scene.background.x);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            if (wm && wm->app) {
                if (wm->app->input.handleMouseButton(msg, lp, wm->app->cam_ctrl, hwnd)) {
                    wm->app->dirty = true;
                }
            }
            return 0;
        case WM_MOUSEMOVE: {
            if (wm && wm->app) {
                RECT rc;
                GetClientRect(hwnd, &rc);
                int vp_w = rc.right - rc.left;
                int vp_h = rc.bottom - rc.top;
                if (wm->app->input.handleMouseMove(lp, wm->app->cam_ctrl, hwnd, vp_w, vp_h)) {
                    wm->app->cam_ctrl.applyTo(wm->app->renderer.ont_scene.camera);
                    wm->app->camera_moving = true;
                    wm->app->camera_stop_tick = GetTickCount();
                    wm->app->dirty = true;
                }
            }
            return 0;
        }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ============================================================================
// Initialization
// ============================================================================
bool WindowManager::init(HINSTANCE hInst, VisorApp* app_instance) {
    this->app = app_instance;
    createGdiResources();
    
    // Classes
    const char* CLASS_MAIN = "MGMain";
    const char* CLASS_VP = "MGViewport";
    const char* CLASS_TB = "MGToolbar";
    const char* CLASS_PR = "MGProps";

    auto registerCls = [&](const char* name, WNDPROC proc, HBRUSH bg) {
        WNDCLASS wc = {};
        wc.lpfnWndProc = proc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = bg;
        wc.lpszClassName = name;
        RegisterClass(&wc);
    };

    registerCls(CLASS_MAIN, WndProc, brush_main);
    registerCls(CLASS_VP, ViewportProc, nullptr); // No background brush — Vulkan swapchain would be overwritten
    registerCls(CLASS_TB, ToolbarProc, brush_header);
    registerCls(CLASS_PR, PropsProc, brush_panel);

    // Main window
    hwnd = CreateWindowEx(0, CLASS_MAIN, "Motor Grafico",
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1280, 720,
        nullptr, nullptr, hInst, this);
    if (!hwnd) return false;

    // Children
    hwnd_toolbar = CreateWindowEx(0, CLASS_TB, nullptr, WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd, nullptr, hInst, this);
        
    hwnd_viewport = CreateWindowEx(0, CLASS_VP, nullptr, WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd, nullptr, hInst, this);
        
    hwnd_props = CreateWindowEx(0, CLASS_PR, nullptr, WS_CHILD | WS_VISIBLE,
        0, 0, 0, 0, hwnd, nullptr, hInst, this);
        
    // Console Edit
    hwnd_console = CreateWindowEx(0, "EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN | ES_READONLY,
        0, 0, 0, 0, hwnd, nullptr, hInst, nullptr);
    SendMessage(hwnd_console, WM_SETFONT, (WPARAM)font_mono, FALSE);
    
    // Status
    hwnd_status = CreateWindowEx(0, "STATIC", 
        " Ready. F1: mouse look | Mouse drag: orbit | +/-: zoom | F5: render | WASD+QE: free fly",
        WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
        0, 0, 0, 0, hwnd, nullptr, hInst, nullptr);
    SendMessage(hwnd_status, WM_SETFONT, (WPARAM)font_ui, FALSE);

    // Initial layout
    updateLayout();

    std::string init_log = 
        "> Initializing Engine Workspace...\n"
        "> Loading scene layout...\n"
        "> Building BVH...\n"
        "> Workspace ready.\n";
    SetWindowText(hwnd_console, init_log.c_str());

    ShowWindow(hwnd, SW_SHOW);
    return true;
}

void WindowManager::destroy() {
    destroyGdiResources();
}

void WindowManager::pumpMessages() {
    MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void WindowManager::paintViewport(HDC hdc, void* pixels, int width, int height, const float* bg_color) {
    RECT rc;
    GetClientRect(hwnd_viewport, &rc);

    if (app && app->renderer.use_vulkan && app->renderer.ont_mode) {
        // Vulkan mode: skip GDI pixel blit, just draw overlay
        fillRect(hdc, {0, 0, rc.right, 28}, RGB(20,20,20));
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255,255,0));
        SelectObject(hdc, font_title);
        if (app->fps_text[0]) {
            TextOutA(hdc, 8, 4, app->fps_text, (int)strlen(app->fps_text));
        }
        return;
    }

    // Draw viewport background (dark)
    fillRect(hdc, rc, UIColor::VIEWPORT_BG);

    if (!pixels || width <= 0 || height <= 0) return;

    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    int vp_w = rc.right - rc.left;
    int vp_h = rc.bottom - rc.top;

    // Stretch low-res render to fill viewport (fast bilinear in GDI)
    // When scale=1.0, this is a 1:1 copy with no stretching
    StretchDIBits(hdc, 0, 0, vp_w, vp_h,
                  0, 0, width, height,
                  pixels, &bmi, DIB_RGB_COLORS, SRCCOPY);

    // Draw FPS overlay
    if (app && app->fps_text[0]) {
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, UIColor::ACCENT_GREEN);
        SelectObject(hdc, font_mono);
        TextOutA(hdc, 8, 8, app->fps_text, (int)strlen(app->fps_text));
    }
}

void WindowManager::drawFpsOverlay() {
    if (!app || !hwnd_viewport) return;
    HDC hdc = GetDC(hwnd_viewport);
    if (!hdc) return;
    RECT rc;
    GetClientRect(hwnd_viewport, &rc);
    int vp_w = rc.right - rc.left;
    int vp_h = rc.bottom - rc.top;
    // Visible indicator: fill top-left corner with semi-transparent dark bar
    fillRect(hdc, {0, 0, rc.right, 28}, RGB(20,20,20));
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(255,255,0));
    SelectObject(hdc, font_title);
    TextOutA(hdc, 8, 4, app->fps_text, (int)strlen(app->fps_text));
    ReleaseDC(hwnd_viewport, hdc);
}

void WindowManager::setWindowTitle(const char* title) {
    SetWindowText(hwnd, title);
}

void WindowManager::updateProps(const char* scene_info) {
    props_text = scene_info;
    if (hwnd_props) {
        InvalidateRect(hwnd_props, nullptr, FALSE);
        UpdateWindow(hwnd_props);
    }
}

} // namespace mg
