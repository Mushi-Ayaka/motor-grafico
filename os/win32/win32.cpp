#include "../os.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace mg {

static Window* g_window_map = nullptr;

static LRESULT CALLBACK WndProcStub(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Window* self = g_window_map;
    if (!self || self->hwnd != hwnd) return DefWindowProc(hwnd, msg, wp, lp);
    switch (msg) {
        case WM_CLOSE:
            self->running = false;
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_SIZE:
            self->width  = LOWORD(lp);
            self->height = HIWORD(lp);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

bool Window::open(int w, int h, const char* title) {
    close();

    HINSTANCE inst = GetModuleHandleA(nullptr);
    const char* CLASS_NAME = "MG_OS_Window";

    WNDCLASSA wc = {};
    wc.lpfnWndProc   = WndProcStub;
    wc.hInstance     = inst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = CLASS_NAME;
    if (!RegisterClassA(&wc)) {
        DWORD err = GetLastError();
        if (err != ERROR_CLASS_ALREADY_EXISTS) return false;
    }

    RECT rc = {0, 0, w, h};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hw = CreateWindowExA(0, CLASS_NAME, title, WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              rc.right - rc.left, rc.bottom - rc.top,
                              nullptr, nullptr, inst, nullptr);
    if (!hw) return false;

    hwnd    = hw;
    hinst   = inst;
    width   = w;
    height  = h;
    running = true;
    g_window_map = this;

    ShowWindow(hw, SW_SHOW);
    return true;
}

bool Window::pump() {
    MSG msg;
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) running = false;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return running;
}

void Window::close() {
    if (hwnd) {
        DestroyWindow((HWND)hwnd);
        hwnd = nullptr;
    }
    if (g_window_map == this) g_window_map = nullptr;
    running = false;
}

Window* Window::self(void* h) {
    if (g_window_map && g_window_map->hwnd == h) return g_window_map;
    return nullptr;
}

} // namespace mg
