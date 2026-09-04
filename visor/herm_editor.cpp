// herm_editor.cpp - Editor de codigo .herm con line numbers, file I/O, error markers.
#include "herm_editor.h"
#include "imgui.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#endif

namespace mg {

// ============================================================================
// Default source
// ============================================================================
static const char* DEFAULT_HERM = R"(# Escena de ejemplo - Lenguaje Hermetico
# Edita y pulsa Ctrl+S o el boton Compile

scene {
  camera {
    position: [0, 2, 8]
    target: [0, 0, 0]
    fov: 50
  }
  background: [0.1, 0.1, 0.2]
}

material rojo {
  base_color: [0.8, 0.2, 0.1]
}

material azul {
  base_color: [0.1, 0.2, 0.8]
}

sdf esfera {
  type: sphere
  params: [1.0]
  material: rojo
  translate: [0, 0, 0]
}

sdf caja {
  type: box
  params: [0.8, 0.8, 0.8]
  material: azul
  translate: [2.5, 0, 0]
}
)";

void HermEditor::initDefault() {
    source = DEFAULT_HERM;
    file_path.clear();
    modified = false;
    errors.clear();
    has_errors = false;
    text_buf_dirty = true;
}

// ============================================================================
// File I/O
// ============================================================================

bool HermEditor::openFile(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    source = ss.str();
    file_path = path;
    modified = false;
    errors.clear();
    has_errors = false;
    text_buf_dirty = true;
    return true;
}

bool HermEditor::saveFile(const char* path) {
    const char* target = path ? path : file_path.c_str();
    if (!target || !target[0]) return false;
    std::ofstream f(target);
    if (!f.is_open()) return false;
    f << source;
    f.flush();
    bool ok = f.good();
    if (ok) {
        file_path = target;
        modified = false;
    }
    return ok;
}

bool HermEditor::saveAsDialog() {
#ifdef _WIN32
    char filename[MAX_PATH] = {};
    if (!file_path.empty()) {
        strncpy(filename, file_path.c_str(), MAX_PATH - 1);
    }

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = GetActiveWindow();
    ofn.lpstrFilter = "Herm Source (*.herm)\0*.herm\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    ofn.lpstrDefExt = "herm";

    if (GetSaveFileNameA(&ofn)) {
        return saveFile(filename);
    }
#endif
    return false;
}

// ============================================================================
// Syntax: keyword detection
// ============================================================================

static bool isAlpha(char c) { return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'; }

const char* HermEditor::keywordColor(const char* word) {
    if (!word || !word[0]) return nullptr;
    // Scene keywords
    if (strcmp(word,"scene")==0)   return "#569cd6"; // blue
    if (strcmp(word,"camera")==0)  return "#569cd6";
    if (strcmp(word,"sdf")==0)     return "#4ec9b0"; // teal
    if (strcmp(word,"material")==0) return "#c586c0"; // purple
    if (strcmp(word,"def")==0)     return "#c586c0";
    if (strcmp(word,"instance")==0) return "#c586c0";
    if (strcmp(word,"light")==0)   return "#569cd6";
    if (strcmp(word,"group")==0)   return "#569cd6";
    // SDF types
    if (strcmp(word,"sphere")==0)  return "#4ec9b0";
    if (strcmp(word,"box")==0)     return "#4ec9b0";
    if (strcmp(word,"cylinder")==0) return "#4ec9b0";
    if (strcmp(word,"capsule")==0) return "#4ec9b0";
    if (strcmp(word,"torus")==0)   return "#4ec9b0";
    if (strcmp(word,"plane")==0)   return "#4ec9b0";
    if (strcmp(word,"cone")==0)    return "#4ec9b0";
    // Operators
    if (strcmp(word,"union")==0)   return "#dcdcaa";
    if (strcmp(word,"subtract")==0) return "#dcdcaa";
    if (strcmp(word,"intersect")==0) return "#dcdcaa";
    if (strcmp(word,"smooth_union")==0) return "#dcdcaa";
    // Properties
    if (strcmp(word,"type")==0)    return "#9cdcfe";
    if (strcmp(word,"params")==0)  return "#9cdcfe";
    if (strcmp(word,"material")==0) return "#9cdcfe";
    if (strcmp(word,"translate")==0) return "#9cdcfe";
    if (strcmp(word,"rotate")==0)  return "#9cdcfe";
    if (strcmp(word,"scale")==0)   return "#9cdcfe";
    if (strcmp(word,"position")==0) return "#9cdcfe";
    if (strcmp(word,"target")==0)  return "#9cdcfe";
    if (strcmp(word,"fov")==0)     return "#9cdcfe";
    if (strcmp(word,"direction")==0) return "#9cdcfe";
    if (strcmp(word,"color")==0)   return "#9cdcfe";
    if (strcmp(word,"intensity")==0) return "#9cdcfe";
    if (strcmp(word,"base_color")==0) return "#9cdcfe";
    if (strcmp(word,"children")==0) return "#9cdcfe";
    if (strcmp(word,"import")==0)  return "#c586c0";
    return nullptr;
}

// ============================================================================
// Draw
// ============================================================================

void HermEditor::draw() {
    if (!visible) return;

    // Reload text buffer from source if needed
    if (text_buf_dirty) {
        size_t len = source.size();
        if (len >= sizeof(text_buf) - 1) len = sizeof(text_buf) - 1;
        memcpy(text_buf, source.data(), len);
        text_buf[len] = 0;
        text_buf_dirty = false;
    }

    // Toolbar
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));

    // File buttons
    if (ImGui::Button("New")) {
        initDefault();
    }
    ImGui::SameLine();

    if (ImGui::Button("Open")) {
#ifdef _WIN32
        char filename[MAX_PATH] = {};
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = GetActiveWindow();
        ofn.lpstrFilter = "Herm Source (*.herm)\0*.herm\0All Files (*.*)\0*.*\0";
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameA(&ofn)) {
            openFile(filename);
        }
#endif
    }
    ImGui::SameLine();

    bool can_save = !file_path.empty();
    if (!can_save) ImGui::BeginDisabled();
    if (ImGui::Button("Save")) {
        saveFile();
    }
    if (!can_save) ImGui::EndDisabled();
    ImGui::SameLine();

    if (ImGui::Button("Save As...")) {
        saveAsDialog();
    }

    ImGui::SameLine();
    ImGui::Text("|");
    ImGui::SameLine();

    // Compile button
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
    if (ImGui::Button("Compile (Ctrl+S)", ImVec2(120, 0))) {
        source = text_buf;
        if (compile_cb) {
            std::string err;
            modified = false;
            if (!compile_cb(source, &err, compile_user_data)) {
                // Parse error line from err if possible
                HermEditorError e = {};
                e.line = 0;
                size_t pos = err.find("line ");
                if (pos != std::string::npos) {
                    e.line = atoi(err.c_str() + pos + 5);
                }
                e.message = err;
                errors.clear();
                errors.push_back(e);
                has_errors = true;
            } else {
                errors.clear();
                has_errors = false;
            }
        }
    }
    ImGui::PopStyleColor();

    ImGui::SameLine();
    if (modified) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "* modified");
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "saved");
    }

    ImGui::SameLine();
    ImGui::Text(" | %s", file_path.empty() ? "(new)" : file_path.c_str());

    ImGui::PopStyleVar(); // FramePadding

    ImGui::Separator();

    // Line numbers + text editor
    ImGui::BeginChild("##editor_scroll", ImVec2(-FLT_MIN, text_height), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);

    // Count lines in buffer
    int line_count = 1;
    for (int i = 0; text_buf[i]; i++) {
        if (text_buf[i] == '\n') line_count++;
    }

    // Get cursor line
    // (simplified: just show line numbers gutter)

    // Line numbers gutter
    float line_num_width = ImGui::CalcTextSize("9999").x + 8.0f;
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));

    // We'll draw the editor in two columns: line numbers + text
    ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
    ImVec2 text_area_pos = ImVec2(cursor_pos.x + line_num_width + 4.0f, cursor_pos.y);

    // Draw line numbers
    ImGui::SetCursorScreenPos(cursor_pos);
    ImGui::BeginGroup();
    for (int i = 1; i <= line_count; i++) {
        ImGui::Text("%4d", i);
        if (i < line_count) {
            // Match line height
            float line_h = ImGui::GetTextLineHeight();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 0); // no extra spacing
        }
    }
    ImGui::EndGroup();

    ImGui::PopStyleColor();

    // Draw text editor next to line numbers
    ImGui::SetCursorScreenPos(text_area_pos);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x);
    if (ImGui::InputTextMultiline("##code", text_buf, sizeof(text_buf),
                                   ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y),
                                   ImGuiInputTextFlags_AllowTabInput)) {
        modified = true;
        // Auto-sync to source on edit
        source = text_buf;
    }
    ImGui::PopItemWidth();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(); // ItemSpacing

    ImGui::EndChild();

    // Error display
    if (has_errors && !errors.empty()) {
        ImGui::Separator();
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        for (auto& e : errors) {
            if (e.line > 0)
                ImGui::Text("Line %d: %s", e.line, e.message.c_str());
            else
                ImGui::TextWrapped("%s", e.message.c_str());
        }
        ImGui::PopStyleColor();
    }

    // Keyboard shortcuts
    ImGuiIO& io = ImGui::GetIO();
    bool ctrl = io.KeyCtrl;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {
        source = text_buf;
        if (compile_cb) {
            std::string err;
            modified = false;
            if (!compile_cb(source, &err, compile_user_data)) {
                HermEditorError e = {};
                e.line = 0;
                size_t pos = err.find("line ");
                if (pos != std::string::npos) {
                    e.line = atoi(err.c_str() + pos + 5);
                }
                e.message = err;
                errors.clear();
                errors.push_back(e);
                has_errors = true;
            } else {
                errors.clear();
                has_errors = false;
            }
        }
    }
}

void HermEditor::setCompileCallback(CompileCallback cb, void* user_data) {
    compile_cb = cb;
    compile_user_data = user_data;
}

} // namespace mg
