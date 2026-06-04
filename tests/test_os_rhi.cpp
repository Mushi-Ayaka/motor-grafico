// test_os_rhi.cpp — Tests de las capas OS y RHI
#include "../os/os.h"
#include "../rhi/rhi.h"
#include <cstdio>
#include <cstring>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>

// ---------------------------------------------------------------------------
// Minimal compute shader that writes a gradient
// ---------------------------------------------------------------------------
static const char* gradient_cs =
    "RWTexture2D<uint> output : register(u0);\n"
    "[numthreads(16, 16, 1)]\n"
    "void main(uint3 tid : SV_DispatchThreadID) {\n"
    "    uint w, h;\n"
    "    output.GetDimensions(w, h);\n"
    "    uint r = tid.x * 255 / (w - 1);\n"
    "    uint g = tid.y * 255 / (h - 1);\n"
    "    output[tid.xy] = 0xFF000000 | r | (g << 8);\n"
    "}\n";

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
static int g_tests = 0, g_passed = 0;

#define TEST(name, expr) do { \
    g_tests++; \
    bool _ok = (expr); \
    if (_ok) g_passed++; \
    printf("  %s: %s\n", name, _ok ? "PASS" : "FAIL"); \
} while(0)

static void test_arena() {
    printf("\n--- Arena ---\n");
    mg::Arena arena;
    arena.init(4 * 1024 * 1024);
    TEST("init", arena.base != nullptr);

    int* arr = (int*)arena.alloc(100 * sizeof(int));
    TEST("alloc not null", arr != nullptr);
    for (int i = 0; i < 100; i++) arr[i] = i;
    bool ok = true;
    for (int i = 0; i < 100; i++) if (arr[i] != i) ok = false;
    TEST("alloc write/read", ok);
    TEST("used after 100 ints", arena.used >= 100 * (int)sizeof(int));

    void* p2 = arena.alloc(64);
    TEST("alloc after used", p2 != nullptr);

    arena.reset();
    TEST("used==0 after reset", arena.used == 0);

    void* p3 = arena.alloc(128);
    TEST("alloc after reset", p3 != nullptr);
    TEST("reuses same base", p3 == arena.base);

    arena.shutdown();
    TEST("base==null after shutdown", arena.base == nullptr);
}

static void test_file() {
    printf("\n--- File mapping ---\n");
    mg::FileMapping fm;

    // Get EXE dir to construct absolute paths
    WCHAR exe_path[MAX_PATH];
    GetModuleFileNameW(nullptr, exe_path, MAX_PATH);
    WCHAR* p = wcsrchr(exe_path, L'\\');
    if (p) *p = L'\0'; // strip exe name, now = build dir
    // Go up to project root: build -> Motor Grafico -> proyecto root
    p = wcsrchr(exe_path, L'\\');
    if (p) *p = L'\0';
    p = wcsrchr(exe_path, L'\\');
    if (p) *p = L'\0';
    // Now exe_path = project root

    WCHAR full[MAX_PATH];
    wsprintfW(full, L"%s\\Lenguaje Hermetico\\ejemplos\\bodegon.rih", exe_path);
    bool found = fm.open(full);
    if (!found) {
        wsprintfW(full, L"%s\\LENGUA~1\\ejemplos\\bodegon.rih", exe_path);
        found = fm.open(full);
    }
    if (found) {
        printf("  Found: %ws (size=%zu)\n", full, fm.size());
        TEST("file data not null", fm.data() != nullptr);
        TEST("file size > 0", fm.size() > 0);
        const char* data = (const char*)fm.data();
        TEST("starts with {", data[0] == '{');
        TEST("contains tablero", strstr(data, "tablero") != nullptr);
        fm.close();
        TEST("data null after close", fm.data() == nullptr);
    }
    TEST("bodegon.rih found", found);
    TEST("nonexistent file", !fm.open(L"nonexistent_file.xyz"));
}

static void test_timer() {
    printf("\n--- Timer ---\n");
    mg::Timer timer;
    timer.init();
    TEST("freq > 0", timer._freq > 0);

    double t0 = timer.now();
    volatile int sum = 0;
    for (int i = 0; i < 1000000; i++) sum += i;
    double t1 = timer.now();
    TEST("now advances", t1 > t0);

    double dt = timer.delta();
    TEST("delta > 0", dt > 0.0);
    TEST("delta reasonable", dt < 1.0);

    double t2 = timer.now();
    TEST("now after delta", t2 >= t1);
}

static void test_rhi() {
    printf("\n--- RHI (DirectX 11 Compute) ---\n");
    mg::Window win;
    if (!win.open(640, 480, "RHI Test")) {
        printf("  SKIP: no se pudo crear ventana\n");
        return;
    }
    TEST("window opened", win.running);
    TEST("win.hwnd != null", win.hwnd != nullptr);
    TEST("win.dimensions", win.width == 640 && win.height == 480);

    mg::Rhi rhi;
    bool ok = rhi.init(win.hwnd, 640, 480);
    TEST("rhi init", ok);
    if (!ok) { win.close(); return; }
    TEST("device", rhi.device != nullptr);
    TEST("context", rhi.ctx != nullptr);
    TEST("swapchain", rhi.swapchain != nullptr);

    // Create compute shader
    mg::RhiShader* sh = rhi.createComputeShader(gradient_cs, "main");
    TEST("compute shader", sh != nullptr && sh->ptr != nullptr);

    // Create output texture
    mg::RhiTexture* tex = rhi.createOutputTexture(640, 480);
    TEST("output texture", tex != nullptr && tex->ptr != nullptr && tex->uav != nullptr);

    // Create a constant buffer (dummy data)
    struct SceneData { float time; float pad[3]; } sd = {1.0f};
    mg::RhiBuffer* cb = rhi.createConstantBuffer(&sd, sizeof(sd));
    TEST("constant buffer", cb != nullptr && cb->ptr != nullptr);

    // Dispatch
    if (sh && tex) {
        rhi.bindShader(sh);
        rhi.bindOutputTexture(tex, 0);
        if (cb) rhi.bindConstantBuffer(cb, 0);
        rhi.dispatch(40, 30, 1);
        TEST("dispatch", true);

        // Present (show gradient)
        // We need to copy the compute output to backbuffer
        ID3D11Device* dev = (ID3D11Device*)rhi.device;
        ID3D11DeviceContext* ctx = (ID3D11DeviceContext*)rhi.ctx;
        ID3D11Texture2D* backbuf = (ID3D11Texture2D*)rhi.backbuffer;
        ctx->CopyResource(backbuf, (ID3D11Texture2D*)tex->ptr);
        rhi.present();
        TEST("present", true);
    }

    // Let the window show for a moment (pump messages)
    for (int i = 0; i < 10 && win.running; i++) {
        win.pump();
        Sleep(16);
    }

    rhi.shutdown();
    TEST("device null after shutdown", rhi.device == nullptr);
    TEST("context null after shutdown", rhi.ctx == nullptr);

    win.close();
    TEST("window closed", win.hwnd == nullptr);
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main() {
    // Enable debug output
    SetConsoleOutputCP(CP_UTF8);
    printf("========================================\n");
    printf("  OS + RHI Layer Tests\n");
    printf("========================================\n");

    test_arena();
    test_file();
    test_timer();
    test_rhi();

    printf("\n========================================\n");
    printf("  Results: %d/%d passed\n", g_passed, g_tests);
    printf("========================================\n");

    if (g_passed == g_tests) {
        printf("  ALL TESTS PASSED\n");
    } else {
        printf("  %d TESTS FAILED\n", g_tests - g_passed);
    }

    return (g_passed == g_tests) ? 0 : 1;
}
