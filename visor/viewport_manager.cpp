#include "viewport_manager.h"
#include "imgui.h"
#include <cstdio>

namespace mg {

void ViewportManager::setDefaultDS(VkDescriptorSet ds) {
    default_ds = ds;
    // Update all tabs that don't have their own DS
    for (auto& tab : tabs) {
        if (tab.ds == VK_NULL_HANDLE) {
            tab.ds = ds;
        }
    }
}

void ViewportManager::addTab(const char* name, ViewportMode mode) {
    ViewportTab tab;
    tab.name = name;
    tab.mode = mode;
    tab.ds = default_ds;  // Use default for now
    tab.dirty = true;
    tabs.push_back(tab);
    active_tab = (int)tabs.size() - 1;
}

void ViewportManager::removeTab(int idx) {
    if (idx >= 0 && idx < (int)tabs.size()) {
        tabs.erase(tabs.begin() + idx);
        if (active_tab >= (int)tabs.size()) {
            active_tab = (int)tabs.size() - 1;
        }
    }
}

void ViewportManager::drawTabBar() {
    // Tab bar
    if (ImGui::BeginTabBar("##viewport_tabs")) {
        for (int i = 0; i < (int)tabs.size(); i++) {
            ImGuiTabBarFlags flags = ImGuiTabBarFlags_NoCloseWithMiddleMouseButton;
            if (ImGui::BeginTabItem(tabs[i].name.c_str(), nullptr, flags)) {
                active_tab = i;
                ImGui::EndTabItem();
            }
        }

        // Add tab button
        if (ImGui::TabItemButton("+")) {
            char name[32];
            snprintf(name, sizeof(name), "View %d", next_tab_id++);
            addTab(name, ViewportMode::RENDER);
        }

        ImGui::EndTabBar();
    }
}

void ViewportManager::drawViewportContent(ViewportTab& tab) {
    if (tab.ds) {
        ImVec2 avail = ImGui::GetContentRegionAvail();
        if (tab.width > 0 && tab.height > 0) {
            float aspect = (float)tab.width / (float)tab.height;
            float img_w = avail.x;
            float img_h = img_w / aspect;
            if (img_h > avail.y) { img_h = avail.y; img_w = img_h * aspect; }
            ImGui::Image((ImTextureID)tab.ds, ImVec2(img_w, img_h));
        } else {
            ImGui::Image((ImTextureID)tab.ds, avail);
        }
    } else {
        ImGui::TextDisabled("No render target");
    }
}

void ViewportManager::draw() {
    if (!visible) return;

    if (ImGui::Begin("Viewport", &visible)) {
        // Ensure at least one tab
        if (tabs.empty()) {
            addTab("Render", ViewportMode::RENDER);
        }

        // Draw tab bar
        drawTabBar();

        // Draw active viewport
        if (active_tab >= 0 && active_tab < (int)tabs.size()) {
            drawViewportContent(tabs[active_tab]);
        }

        // Mode indicator
        if (active_tab >= 0 && active_tab < (int)tabs.size()) {
            const char* mode_str = "?";
            switch (tabs[active_tab].mode) {
                case ViewportMode::RENDER: mode_str = "Render"; break;
                case ViewportMode::IR:     mode_str = "IR/Topology"; break;
                case ViewportMode::TENSOR: mode_str = "Tensor"; break;
            }
            ImGui::Text("Mode: %s", mode_str);
        }
    }
    ImGui::End();
}

} // namespace mg
