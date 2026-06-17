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
};

extern VulkanContext vk_ctx;

} // namespace mg
