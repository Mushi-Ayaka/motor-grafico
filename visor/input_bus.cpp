// input_bus.cpp - T-110: Input Bus unificado.
#include "input_bus.h"
#include "imgui.h"

namespace mg {

bool InputBus::ImGuiWantMouse() {
    return ImGui::GetCurrentContext() ? ImGui::GetIO().WantCaptureMouse : false;
}

bool InputBus::ImGuiWantKeyboard() {
    return ImGui::GetCurrentContext() ? ImGui::GetIO().WantCaptureKeyboard : false;
}

} // namespace mg
