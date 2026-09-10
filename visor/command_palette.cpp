#include "command_palette.h"
#include "imgui.h"
#include <cstring>
#include <algorithm>

namespace mg {

void CommandPalette::toggle() {
    visible = !visible;
    if (visible) {
        search_buf[0] = '\0';
        selected_idx = 0;
        filterCommands();
    }
}

void CommandPalette::registerCommand(const char* name, const char* category, const char* shortcut, std::function<void()> action) {
    Command cmd;
    cmd.name = name;
    cmd.category = category;
    cmd.shortcut = shortcut;
    cmd.action = action;
    commands.push_back(cmd);
}

void CommandPalette::filterCommands() {
    filtered_indices.clear();
    std::string search = search_buf;
    std::transform(search.begin(), search.end(), search.begin(), ::tolower);

    for (int i = 0; i < (int)commands.size(); i++) {
        std::string name_lower = commands[i].name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);

        // Simple substring match
        if (search.empty() || name_lower.find(search) != std::string::npos) {
            filtered_indices.push_back(i);
        }
    }
    selected_idx = 0;
}

void CommandPalette::draw() {
    if (!visible) return;

    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Command Palette", &visible)) {
        // Search input
        ImGui::SetKeyboardFocusHere();
        if (ImGui::InputText("##search", search_buf, sizeof(search_buf),
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (selected_idx >= 0 && selected_idx < (int)filtered_indices.size()) {
                int cmd_idx = filtered_indices[selected_idx];
                if (commands[cmd_idx].action) {
                    commands[cmd_idx].action();
                }
                visible = false;
            }
        }

        // Filter on text change
        static char last_search[256] = {};
        if (strcmp(last_search, search_buf) != 0) {
            filterCommands();
            strcpy(last_search, search_buf);
        }

        ImGui::Separator();

        // Command list
        ImGui::BeginChild("##commands", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

        for (int i = 0; i < (int)filtered_indices.size(); i++) {
            int cmd_idx = filtered_indices[i];
            const Command& cmd = commands[cmd_idx];

            bool is_selected = (i == selected_idx);

            // Highlight selected
            if (is_selected) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
            }

            // Category prefix
            char label[256];
            if (!cmd.shortcut.empty()) {
                snprintf(label, sizeof(label), "[%s] %s  (%s)", cmd.category.c_str(), cmd.name.c_str(), cmd.shortcut.c_str());
            } else {
                snprintf(label, sizeof(label), "[%s] %s", cmd.category.c_str(), cmd.name.c_str());
            }

            if (ImGui::Selectable(label, is_selected)) {
                if (cmd.action) {
                    cmd.action();
                }
                visible = false;
            }

            if (is_selected) {
                ImGui::PopStyleColor();
            }

            // Update selected on click
            if (ImGui::IsItemHovered()) {
                selected_idx = i;
            }
        }

        ImGui::EndChild();

        // Keyboard navigation
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
            selected_idx = (selected_idx > 0) ? selected_idx - 1 : (int)filtered_indices.size() - 1;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
            selected_idx = (selected_idx < (int)filtered_indices.size() - 1) ? selected_idx + 1 : 0;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            visible = false;
        }
    }
    ImGui::End();
}

void CommandPalette::buildDefaultCommands() {
    // File commands
    registerCommand("New Project", "File", "Ctrl+N", nullptr);
    registerCommand("Open Project", "File", "Ctrl+Shift+O", nullptr);
    registerCommand("Open .herm", "File", "Ctrl+O", nullptr);
    registerCommand("Save Project", "File", "Ctrl+S", nullptr);
    registerCommand("Save Project As", "File", "Ctrl+Shift+S", nullptr);
    registerCommand("Export", "File", "Ctrl+E", nullptr);
    registerCommand("Exit", "File", "", nullptr);

    // Edit commands
    registerCommand("Undo", "Edit", "Ctrl+Z", nullptr);
    registerCommand("Redo", "Edit", "Ctrl+Y", nullptr);

    // View commands
    registerCommand("Toggle Console", "View", "", nullptr);
    registerCommand("Toggle Editor", "View", "", nullptr);
    registerCommand("Toggle Profiler", "View", "", nullptr);
    registerCommand("Toggle Inspector", "View", "", nullptr);
    registerCommand("Toggle Ontology Tree", "View", "", nullptr);
    registerCommand("Toggle Tensor Inspector", "View", "", nullptr);
    registerCommand("Toggle Source Browser", "View", "Ctrl+B", nullptr);
    registerCommand("Toggle Gizmos", "View", "", nullptr);

    // Mode commands
    registerCommand("Mode: GPU", "Mode", "", nullptr);
    registerCommand("Mode: CPU", "Mode", "", nullptr);
    registerCommand("Gizmo: Move", "Gizmo", "", nullptr);
    registerCommand("Gizmo: Rotate", "Gizmo", "", nullptr);
    registerCommand("Gizmo: Scale", "Gizmo", "", nullptr);

    // Compile
    registerCommand("Compile .herm", "Build", "Ctrl+Enter", nullptr);
}

} // namespace mg
