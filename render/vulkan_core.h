#pragma once
#include <Volk/volk.h>
#include <vma/vk_mem_alloc.h>
#include <windows.h>
#include <vector>

namespace mg {

struct VulkanContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debug_messenger = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VmaAllocator allocator = VK_NULL_HANDLE;
    VkQueue compute_queue = VK_NULL_HANDLE;
    VkQueue graphics_queue = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    uint32_t compute_family_idx = ~0u;
    uint32_t graphics_family_idx = ~0u;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchain_image_format;
    VkExtent2D swapchain_extent;
    std::vector<VkImage> swapchain_images;
    std::vector<VkImageView> swapchain_image_views;

    VkSemaphore image_available_sem = VK_NULL_HANDLE;
    VkSemaphore render_finished_sem = VK_NULL_HANDLE;
    VkFence in_flight_fence = VK_NULL_HANDLE;

    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer compute_command_buffer = VK_NULL_HANDLE;

    bool init(HWND hwnd);
    bool initSwapchain(uint32_t width, uint32_t height);
    void cleanupSwapchain();
    void cleanup();

    bool resizeSwapchain(uint32_t width, uint32_t height);

    // Helpers de memoria
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkBuffer& buffer, VmaAllocation& allocation);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    bool test_pattern = false; // when true, fills swapchain with solid red (bypasses compute)

    bool drawFrame(class VulkanSceneData& scene_data, uint32_t render_w, uint32_t render_h);

    // --- ImGui overlay (T-F6) -------------------------------------------------
    VkRenderPass    imgui_render_pass    = VK_NULL_HANDLE;
    VkDescriptorPool imgui_descriptor_pool = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> imgui_framebuffers;
    VkCommandPool   imgui_command_pool   = VK_NULL_HANDLE;
    VkCommandBuffer imgui_command_buffer  = VK_NULL_HANDLE;
    VkSemaphore     imgui_present_sem     = VK_NULL_HANDLE; // graphics -> present
    bool            imgui_enabled         = false;

    bool initImGui(HWND hwnd);
    void createImguiFramebuffers();
    void shutdownImGui();

    // --- Viewport image (T-102): offscreen render target for ImGui::Image -----
    VkImage         viewport_image        = VK_NULL_HANDLE;
    VkImageView     viewport_image_view   = VK_NULL_HANDLE;
    VmaAllocation   viewport_image_alloc  = VK_NULL_HANDLE;
    VkSampler       viewport_sampler      = VK_NULL_HANDLE;
    VkDescriptorPool viewport_ds_pool     = VK_NULL_HANDLE;
    VkDescriptorSetLayout viewport_ds_layout = VK_NULL_HANDLE;
    VkDescriptorSet viewport_ds           = VK_NULL_HANDLE;
    uint32_t        viewport_w            = 0;
    uint32_t        viewport_h            = 0;

    bool initViewportImage(uint32_t w, uint32_t h);
    void destroyViewportImage();
    void copyOutputToViewport(VkCommandBuffer cmd, VkBuffer output_buffer, uint32_t w, uint32_t h);
};

extern VulkanContext vk_ctx;

} // namespace mg
