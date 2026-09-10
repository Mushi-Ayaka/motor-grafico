#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "../render/vulkan_core.h"

namespace mg {

enum class ViewportMode : uint8_t {
    RENDER = 0,     // Normal SDF render
    IR = 1,         // IR/Topology view
    TENSOR = 2      // Tensor visualization
};

struct ViewportTab {
    std::string name = "Viewport";
    ViewportMode mode = ViewportMode::RENDER;
    VkDescriptorSet ds = VK_NULL_HANDLE;  // ImGui descriptor set for this viewport
    uint32_t width = 0;
    uint32_t height = 0;
    bool dirty = true;
};

class ViewportManager {
public:
    bool visible = true;
    std::vector<ViewportTab> tabs;
    int active_tab = 0;
    int next_tab_id = 0;

    // Default viewport image (from vk_ctx)
    VkDescriptorSet default_ds = VK_NULL_HANDLE;

    void draw();
    void addTab(const char* name, ViewportMode mode = ViewportMode::RENDER);
    void removeTab(int idx);
    void setDefaultDS(VkDescriptorSet ds);

private:
    void drawTabBar();
    void drawViewportContent(ViewportTab& tab);
};

} // namespace mg
