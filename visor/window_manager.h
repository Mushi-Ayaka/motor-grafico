#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

namespace mg {

struct VisorApp;

// ============================================================================
// Color palette (dark graphics engine theme)
// ============================================================================
namespace UIColor {
    static const COLORREF BG_MAIN       = RGB(15, 15, 20);
    static const COLORREF BG_PANEL      = RGB(22, 22, 28);
    static const COLORREF BG_TOOLBAR    = RGB(28, 28, 36);
    static const COLORREF BG_HEADER     = RGB(35, 35, 45);
    static const COLORREF BG_EDIT       = RGB(12, 12, 16);
    static const COLORREF BORDER        = RGB(50, 50, 65);
    static const COLORREF BORDER_BRIGHT = RGB(80, 80, 110);
    static const COLORREF TEXT_PRIMARY  = RGB(220, 220, 235);
    static const COLORREF TEXT_SECONDARY= RGB(140, 140, 165);
    static const COLORREF TEXT_DIM      = RGB(80, 80, 100);
    static const COLORREF ACCENT_BLUE   = RGB(80, 150, 255);
    static const COLORREF ACCENT_ORANGE = RGB(255, 150, 60);
    static const COLORREF ACCENT_GREEN  = RGB(80, 220, 140);
    static const COLORREF VIEWPORT_BG   = RGB(8, 8, 12);
    static const COLORREF BTN_NORMAL    = RGB(45, 45, 58);
    static const COLORREF BTN_HOVER     = RGB(60, 60, 80);
    static const COLORREF BTN_ACTIVE    = RGB(80, 150, 255);
}

// ============================================================================
// Layout constants
// ============================================================================
static const int UI_TOOLBAR_H = 40;
static const int UI_STATUS_H  = 24;
static const int UI_PANEL_W   = 290;
static const int UI_CONSOLE_H = 180;

// ============================================================================
// Window manager
// ============================================================================
struct WindowManager {
    // Handles
    HWND hwnd           = nullptr;
    HWND hwnd_viewport  = nullptr;
    HWND hwnd_console   = nullptr;
    HWND hwnd_props     = nullptr;
    HWND hwnd_toolbar   = nullptr;
    HWND hwnd_status    = nullptr;

    VisorApp* app = nullptr;

    // GDI resources
    HBRUSH brush_main   = nullptr;
    HBRUSH brush_panel  = nullptr;
    HBRUSH brush_edit   = nullptr;
    HBRUSH brush_header = nullptr;
    HFONT  font_ui      = nullptr;
    HFONT  font_mono    = nullptr;
    HFONT  font_title   = nullptr;

    // State
    int hovered_btn = -1;  // toolbar button hover state
    std::string props_text;

    bool init(HINSTANCE hInst, VisorApp* app_instance);
    void destroy();
    void pumpMessages();
    void paintViewport(HDC hdc, void* pixels, int width, int height, const float* bg_color);
    void drawFpsOverlay();
    void setWindowTitle(const char* title);
    void updateProps(const char* scene_info);

    // Internal
    void createGdiResources();
    void destroyGdiResources();
    void updateLayout();

    static void drawPanelHeader(HDC hdc, RECT rc, const char* label, COLORREF accent);
    static void drawSeparator(HDC hdc, int x, int y, int w);
    static void drawStatRow(HDC hdc, HFONT font, int x, int& y, const char* key, const char* val, COLORREF val_color = UIColor::TEXT_PRIMARY);
    static void fillRect(HDC hdc, RECT rc, COLORREF c);
    static void drawRect(HDC hdc, RECT rc, COLORREF c);

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK ViewportProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK ToolbarProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK PropsProc(HWND, UINT, WPARAM, LPARAM);
};

} // namespace mg
