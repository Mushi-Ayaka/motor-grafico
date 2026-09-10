#include "source_browser.h"
#include "imgui.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlwapi.h>
#include <algorithm>

namespace mg {

void SourceBrowser::scanDirectory(const std::string& path, FileEntry& parent) {
    parent.children.clear();
    parent.path = path;

    WIN32_FIND_DATAA findData;
    std::string searchPath = path + "\\*";
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0)
            continue;

        FileEntry entry;
        entry.name = findData.cFileName;
        entry.path = path + "\\" + entry.name;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            entry.is_directory = true;
            scanDirectory(entry.path, entry);
        } else {
            // Filter: only show .herm, .mgproj, .txt, .json files
            const char* ext = strrchr(findData.cFileName, '.');
            if (ext) {
                if (_stricmp(ext, ".herm") == 0 ||
                    _stricmp(ext, ".mgproj") == 0 ||
                    _stricmp(ext, ".txt") == 0 ||
                    _stricmp(ext, ".json") == 0 ||
                    _stricmp(ext, ".bat") == 0 ||
                    _stricmp(ext, ".cpp") == 0 ||
                    _stricmp(ext, ".h") == 0) {
                    parent.children.push_back(entry);
                }
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);

    // Sort: directories first, then alphabetical
    std::sort(parent.children.begin(), parent.children.end(),
        [](const FileEntry& a, const FileEntry& b) {
            if (a.is_directory != b.is_directory) return a.is_directory;
            return a.name < b.name;
        });
}

void SourceBrowser::refresh() {
    if (root_path.empty()) return;
    scanDirectory(root_path, root);
    root.name = root_path;
    root.is_directory = true;
    root.is_expanded = true;
}

void SourceBrowser::drawNode(FileEntry& entry, int depth) {
    ImGui::PushID(&entry);

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!entry.is_directory) flags |= ImGuiTreeNodeFlags_Leaf;
    if (&entry == selected) flags |= ImGuiTreeNodeFlags_Selected;

    // Icon
    const char* icon = entry.is_directory ? "[D]" : "[F]";
    bool node_open = ImGui::TreeNodeEx("%s %s", flags, icon, entry.name.c_str());

    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        selected = &entry;
        if (!entry.is_directory && on_open) {
            on_open(entry.path.c_str(), open_user_data);
        }
    }

    if (node_open) {
        if (entry.is_directory) {
            for (auto& child : entry.children) {
                drawNode(child, depth + 1);
            }
        }
        ImGui::TreePop();
    }

    ImGui::PopID();
}

void SourceBrowser::draw() {
    if (!visible) return;

    if (ImGui::Begin("Source Browser", &visible)) {
        // Toolbar
        if (ImGui::Button("Refresh")) {
            refresh();
        }
        ImGui::SameLine();
        if (ImGui::Button("New File")) {
            // TODO: create new .herm file
        }
        ImGui::Separator();

        // File tree
        if (root_path.empty()) {
            ImGui::TextDisabled("No project loaded");
            ImGui::TextDisabled("Open a .mgproj to browse files");
        } else {
            for (auto& child : root.children) {
                drawNode(child, 0);
            }
        }
    }
    ImGui::End();
}

} // namespace mg
