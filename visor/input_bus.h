// input_bus.h - T-110: Input Bus unificado (teclado/ratón + ray-SDF).
// Unifica input de Win32 + ImGui. El editor y Play consumen del mismo bus.
#pragma once
#include <cstdint>
#include <cstring>

namespace mg {

struct InputSnapshot {
    bool keys[256];
    float mouse_dx, mouse_dy;
    float scroll_y;
    bool mouse_left, mouse_right, mouse_middle;
};

struct InputState {
    // Mouse
    float mouse_x = 0, mouse_y = 0;
    float mouse_dx = 0, mouse_dy = 0;
    float scroll_y = 0;
    bool  mouse_left = false;
    bool  mouse_right = false;
    bool  mouse_middle = false;

    // Keyboard (basic: 256 keys)
    bool  keys[256] = {};
    bool  keys_prev[256] = {};  // frame anterior para detectar press/release

    // ImGui state
    bool  imgui_want_mouse = false;
    bool  imgui_want_keyboard = false;

    // Helpers
    bool isKeyPressed(int key) const {
        return keys[key] && !keys_prev[key];
    }
    bool isKeyReleased(int key) const {
        return !keys[key] && keys_prev[key];
    }
    bool isKeyDown(int key) const {
        return keys[key];
    }

    // Copy current → prev (call at start of frame)
    void advanceFrame() {
        memcpy(keys_prev, keys, sizeof(keys));
        scroll_y = 0;
        mouse_dx = 0;
        mouse_dy = 0;
    }

    // Create immutable snapshot for systems
    InputSnapshot snapshot() const {
        InputSnapshot s;
        memcpy(s.keys, keys, sizeof(keys));
        s.mouse_dx = mouse_dx;
        s.mouse_dy = mouse_dy;
        s.scroll_y = scroll_y;
        s.mouse_left = mouse_left;
        s.mouse_right = mouse_right;
        s.mouse_middle = mouse_middle;
        return s;
    }
};

class InputBus {
public:
    InputState state;
    bool play_mode = false;  // separado de ont_mode (GPU/CPU)

    // Llamar al inicio de cada frame
    void beginFrame() {
        state.advanceFrame();
    }

    // Actualizar desde Win32 msg (WM_KEYDOWN/UP, WM_MOUSEMOVE, etc.)
    void onKey(int key, bool down) {
        if (key >= 0 && key < 256)
            state.keys[key] = down;
    }

    void onMouseMove(float x, float y) {
        state.mouse_dx = x - state.mouse_x;
        state.mouse_dy = y - state.mouse_y;
        state.mouse_x = x;
        state.mouse_y = y;
    }

    void onMouseButton(int button, bool down) {
        if (button == 0) state.mouse_left = down;
        if (button == 1) state.mouse_right = down;
        if (button == 2) state.mouse_middle = down;
    }

    void onScroll(float y) {
        state.scroll_y += y;
    }

    // Actualizar flags de ImGui (llamar después de ImGui::NewFrame)
    void updateImGuiFlags() {
        state.imgui_want_mouse = ImGuiWantMouse();
        state.imgui_want_keyboard = ImGuiWantKeyboard();
    }

    // Preguntar a ImGui (stub por ahora)
    static bool ImGuiWantMouse();
    static bool ImGuiWantKeyboard();
};

} // namespace mg
