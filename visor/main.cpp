// Motor Gráfico — Visor 3D interactivo con workspace
// Punto de entrada refactorizado

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include <vector>

#include <cstring>
#include "visor_app.h"
#include "../render/ray_march.h"
#include "../render/ray_march_simd.h"
#include "../render/jit_compiler.h"

using namespace mg;

static FILE* g_bench_log = nullptr;
static void bprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    if (g_bench_log) {
        va_list args2;
        va_start(args2, fmt);
        vfprintf(g_bench_log, fmt, args2);
        va_end(args2);
        fflush(g_bench_log);
    }
}

static int benchmark(const wchar_t* path) {
    fopen_s(&g_bench_log, "benchmark_result.txt", "w");


    FileMapping fm;
    if (!fm.open(path)) {
        bprintf("ERROR: no se pudo abrir %ws\n", path);
        return 1;
    }

    OntScene sc;
    if (!sc.loadOnt(fm)) {
        bprintf("ERROR: no se pudo cargar .ont\n");
        return 1;
    }

    bool run_scalar = false;
    LPCWSTR cmdLine = GetCommandLineW();
    if (cmdLine && wcsstr(cmdLine, L"--scalar") != nullptr) {
        run_scalar = true;
    }

    // Try companion .obs
    std::wstring obs_path(path);
    size_t dot = obs_path.rfind(L'.');
    if (dot != std::wstring::npos) {
        obs_path = obs_path.substr(0, dot) + L".obs";
        FileMapping obs_fm;
        if (obs_fm.open(obs_path.c_str())) {
            sc.loadObs(obs_fm);
            sc.applyObs();
            obs_fm.close();
            bprintf("Cargado .obs: %ws\n", obs_path.c_str());
        }
    }

    JitCompiler jit;
    jit.init();
    jit.compileScene(sc);

    bprintf("Escena: %u nodos, %u BVH, %u materiales, %u bytecode bytes\n",
           sc.header->node_count, sc.header->bvh_count,
           sc.header->material_count, sc.header->bytecode_size);
    bprintf("Camara: pos=(%.2f,%.2f,%.2f) target=(%.2f,%.2f,%.2f) fov=%.1f\n",
           sc.camera.position.x, sc.camera.position.y, sc.camera.position.z,
           sc.camera.target.x, sc.camera.target.y, sc.camera.target.z,
           sc.camera.fov);
    bprintf("Resolucion: %u x %u\n", sc.width, sc.height);

    Arena arena;
    arena.init(64 * 1024 * 1024);
    Frame fb;

    // Render at .obs resolution first
    f32 w = 0.0f;
    fb.init(arena, (int)sc.width, (int)sc.height);
    LARGE_INTEGER freq, t0, t1;
    QueryPerformanceFrequency(&freq);
    f32 ms = 0.0f;

    if (run_scalar) {
        bprintf("Renderizando 800x600 Scalar ST...\n");
        QueryPerformanceCounter(&t0);
        renderOntScene(sc, fb, w);
        QueryPerformanceCounter(&t1);
        ms = (f32)(t1.QuadPart - t0.QuadPart) * 1000.0f / (f32)freq.QuadPart;
        bprintf("Render 800x600 (Scalar ST): %.1f ms\n", ms);

        bprintf("Renderizando 800x600 Scalar MT...\n");
        QueryPerformanceCounter(&t0);
        renderOntSceneMT(sc, fb, w);
        QueryPerformanceCounter(&t1);
        ms = (f32)(t1.QuadPart - t0.QuadPart) * 1000.0f / (f32)freq.QuadPart;
        bprintf("Render 800x600 (Scalar MT): %.1f ms\n", ms);
    }

    // Now at larger resolution (simulating viewport)
    arena.reset();
    int vp_w = 1000, vp_h = 700;
    fb.init(arena, vp_w, vp_h);
    bprintf("\n--- Viewport simulado %dx%d ---\n", vp_w, vp_h);

    if (run_scalar) {
        bprintf("Single-thread:\n");
        QueryPerformanceCounter(&t0);
        renderOntScene(sc, fb, w);
        QueryPerformanceCounter(&t1);
        ms = (f32)(t1.QuadPart - t0.QuadPart) * 1000.0f / (f32)freq.QuadPart;
        bprintf("  Render: %.1f ms (%.1f FPS)\n", ms, 1000.0f / ms);

        bprintf("Multi-thread:\n");
        arena.reset();
        fb.init(arena, vp_w, vp_h);
        QueryPerformanceCounter(&t0);
        renderOntSceneMT(sc, fb, w);
        QueryPerformanceCounter(&t1);
        ms = (f32)(t1.QuadPart - t0.QuadPart) * 1000.0f / (f32)freq.QuadPart;
        bprintf("  Render: %.1f ms (%.1f FPS)\n", ms, 1000.0f / ms);
    }

    if (run_scalar) {
        // Test with camera INSIDE geometry (simulates WASD into scene)
        bprintf("\n--- Camara DENTRO (MT) ---\n");
        sc.camera.position = {0.0f, 0.0f, 0.0f}; // inside sphere at origin
        sc.camera.target = {0.0f, 0.0f, 1.0f};
        arena.reset();
        fb.init(arena, vp_w, vp_h);
        QueryPerformanceCounter(&t0);
        renderOntSceneMT(sc, fb, w);
        QueryPerformanceCounter(&t1);
        ms = (f32)(t1.QuadPart - t0.QuadPart) * 1000.0f / (f32)freq.QuadPart;
        bprintf("  Render: %.1f ms (%.1f FPS)\n", ms, 1000.0f / ms);

        // Test with camera just outside (near surface)
        bprintf("\n--- Camara CERCA de superficie (MT) ---\n");
        sc.camera.position = {0.0f, 0.0f, 1.01f}; // just outside sphere radius 1
        sc.camera.target = {0.0f, 0.0f, 0.0f};
        arena.reset();
        fb.init(arena, vp_w, vp_h);
        QueryPerformanceCounter(&t0);
        renderOntSceneMT(sc, fb, w);
        QueryPerformanceCounter(&t1);
        ms = (f32)(t1.QuadPart - t0.QuadPart) * 1000.0f / (f32)freq.QuadPart;
        bprintf("  Render: %.1f ms (%.1f FPS)\n", ms, 1000.0f / ms);
    }

    // --- SIMD benchmark ---
    bprintf("\n========== SIMD Packet Tracing ==========\n");

    // Restore camera
    sc.camera.position = {0.0f, 0.0f, 3.0f};
    sc.camera.target = {0.0f, 0.0f, 0.0f};

    bprintf("SIMD Single-thread:\n");
    arena.reset();
    fb.init(arena, vp_w, vp_h);
    QueryPerformanceCounter(&t0);
    renderOntSceneSIMD(sc, fb, w, (sc.has_obs && !sc.obs.lights.empty()) ? sc.obs.lights.data() : nullptr,
                       (u32)(sc.has_obs ? sc.obs.lights.size() : 0),
                       sc.has_obs && sc.obs.has_background ? sc.obs.background : sc.background,
                       sc.pipeline);
    QueryPerformanceCounter(&t1);
    ms = (f32)(t1.QuadPart - t0.QuadPart) * 1000.0f / (f32)freq.QuadPart;
    bprintf("  Render: %.1f ms (%.1f FPS)\n", ms, 1000.0f / ms);

    bprintf("SIMD Multi-thread:\n");
    arena.reset();
    fb.init(arena, vp_w, vp_h);
    QueryPerformanceCounter(&t0);
    renderOntSceneMTSIMD(sc, fb, w);
    QueryPerformanceCounter(&t1);
    ms = (f32)(t1.QuadPart - t0.QuadPart) * 1000.0f / (f32)freq.QuadPart;
    bprintf("  Render: %.1f ms (%.1f FPS)\n", ms, 1000.0f / ms);

    // Full HD
    bprintf("\n--- Full HD (1920x1080) ---\n");
    arena.reset();
    fb.init(arena, 1920, 1080);
    QueryPerformanceCounter(&t0);
    renderOntSceneMTSIMD(sc, fb, w);
    QueryPerformanceCounter(&t1);
    ms = (f32)(t1.QuadPart - t0.QuadPart) * 1000.0f / (f32)freq.QuadPart;
    bprintf("  SIMD MT: %.1f ms (%.1f FPS)\n", ms, 1000.0f / ms);

    arena.reset();
    fb.init(arena, 1920, 1080);
    QueryPerformanceCounter(&t0);
    renderOntSceneMT(sc, fb, w);
    QueryPerformanceCounter(&t1);
    ms = (f32)(t1.QuadPart - t0.QuadPart) * 1000.0f / (f32)freq.QuadPart;
    bprintf("  Scalar MT: %.1f ms (%.1f FPS)\n", ms, 1000.0f / ms);

    arena.shutdown();
    fm.close();
    if (g_bench_log) fclose(g_bench_log);
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR lpCmdLine, int) {
    // Check for --bench-vulkan (must check before --bench to avoid substring match)
    bool bench_vulkan = (lpCmdLine && strstr(lpCmdLine, "--bench-vulkan") != nullptr);

    // Check for --bench argument: visor.exe "<ruta.ont>" --bench
    if (lpCmdLine && !bench_vulkan && strstr(lpCmdLine, "--bench") != nullptr) {
        wchar_t path[MAX_PATH] = {0};
        // Parse the first quoted or unquoted arg before --bench
        int len = (int)strlen(lpCmdLine);
        char ont_path_a[MAX_PATH] = {0};
        if (lpCmdLine[0] == '"') {
            // quoted
            const char* end = strchr(lpCmdLine + 1, '"');
            if (end) strncpy_s(ont_path_a, lpCmdLine + 1, end - lpCmdLine - 1);
        } else {
            // unquoted — up to first space
            const char* sp = strchr(lpCmdLine, ' ');
            if (sp) strncpy_s(ont_path_a, lpCmdLine, sp - lpCmdLine);
            else strncpy_s(ont_path_a, lpCmdLine, MAX_PATH - 1);
        }
        if (ont_path_a[0] != '\0')
            MultiByteToWideChar(CP_UTF8, 0, ont_path_a, -1, path, MAX_PATH);
        else {
            wchar_t exe_dir[MAX_PATH];
            GetModuleFileNameW(nullptr, exe_dir, MAX_PATH);
            wchar_t* p = wcsrchr(exe_dir, L'\\');
            if (p) *p = L'\0';
            wcscpy_s(path, exe_dir);
            wcscat_s(path, L"\\test_custom.ont");
        }
        return benchmark(path);
    }

    wchar_t full_path[MAX_PATH] = {0};
    
    // Parse first argument (before --bench-vulkan)
    if (lpCmdLine && lpCmdLine[0] != '\0') {
        char ont_path_a[MAX_PATH] = {0};
        if (lpCmdLine[0] == '"') {
            const char* end = strchr(lpCmdLine + 1, '"');
            if (end) strncpy_s(ont_path_a, lpCmdLine + 1, end - lpCmdLine - 1);
        } else {
            const char* sp = strchr(lpCmdLine, ' ');
            if (sp) {
                size_t len = sp - lpCmdLine;
                // Skip leading spaces
                const char* start = lpCmdLine;
                while (*start == ' ') start++;
                if (start < sp) {
                    len = sp - start;
                    strncpy_s(ont_path_a, start, len);
                }
            } else {
                strncpy_s(ont_path_a, lpCmdLine, MAX_PATH - 1);
            }
        }
        if (ont_path_a[0] != '\0')
            MultiByteToWideChar(CP_UTF8, 0, ont_path_a, -1, full_path, MAX_PATH);
        // If path still empty or file doesn't exist, fall through to defaults
        if (full_path[0] == L'\0' || GetFileAttributesW(full_path) == INVALID_FILE_ATTRIBUTES) {
            full_path[0] = L'\0';
        }
    }
    if (full_path[0] == L'\0') {
        wchar_t exe_dir[MAX_PATH];
        GetModuleFileNameW(nullptr, exe_dir, MAX_PATH);
        wchar_t* p = wcsrchr(exe_dir, L'\\');
        if (p) *p = L'\0';

        wcscpy_s(full_path, exe_dir);
        wcscat_s(full_path, L"\\..\\..\\Lenguaje Hermetico\\libreria\\escenas\\catedral_hermetica_0000.ont");
        if (GetFileAttributesW(full_path) == INVALID_FILE_ATTRIBUTES) {
            wcscpy_s(full_path, exe_dir);
            wcscat_s(full_path, L"\\test_custom.ont");
        }
        if (GetFileAttributesW(full_path) == INVALID_FILE_ATTRIBUTES) {
            wcscpy_s(full_path, exe_dir);
            wcscat_s(full_path, L"\\..\\..\\Lenguaje Hermetico\\libreria\\escenas\\test_suelo.rih");
        }
    }

    VisorApp app;
    if (!app.init(hInst, full_path)) {
        MessageBox(nullptr, "Fallo al inicializar VisorApp", "Error", MB_OK);
        return 1;
    }

    app.bench_vulkan = bench_vulkan;
    app.run();
    return 0;
}
