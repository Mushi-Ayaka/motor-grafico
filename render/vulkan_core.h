#pragma once
#include <volk.h>
#include <vma/vk_mem_alloc.h>
#include <windows.h>
#include <vector>

namespace mg {

// A3: número de frames en vuelo (fences/semaphores/command buffers por slot)
static constexpr uint32_t FRAMES_IN_FLIGHT = 2;

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

    VkSemaphore image_available_sem[FRAMES_IN_FLIGHT] = {};
    VkSemaphore render_finished_sem[FRAMES_IN_FLIGHT] = {};
    VkFence in_flight_fence[FRAMES_IN_FLIGHT] = {};
    VkFence imgui_fence[FRAMES_IN_FLIGHT] = {}; // A3: fence per-frame para el submit de ImGui (graphics queue)
    uint32_t current_frame = 0; // índice de frame en vuelo (ciclo 0/1)

    VkCommandPool command_pool = VK_NULL_HANDLE;
    VkCommandBuffer compute_command_buffer[FRAMES_IN_FLIGHT] = {};

    // --- A1: GPU timestamps (medición rigurosa por dispatch) ---
    VkQueryPool timestamp_pool = VK_NULL_HANDLE;
    float        timestamp_period_ns = 0.0f;   // ns por tick (de VkPhysicalDeviceLimits)
    double       last_gpu_ms = 0.0;            // GPU time del último frame leído (tras fence wait)
    bool         timestamp_submitted[FRAMES_IN_FLIGHT] = {}; // true si el slot ya tuvo un submit con timestamps

    char device_name[256] = {}; // nombre del GPU elegido (para stats)

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
    VkCommandBuffer imgui_command_buffer[FRAMES_IN_FLIGHT] = {}; // A3: per-frame (evita race con frames en vuelo)
    VkSemaphore     imgui_present_sem     = VK_NULL_HANDLE; // graphics -> present
    bool            imgui_enabled         = false;
    bool            ui_visible            = true; // F2: oculta overlay ImGui (barrera -> PRESENT_SRC directo)

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

    // --- Readback/Staging buffer (T-118): GPU→CPU tensor readback -----
    VkBuffer        readback_buffer      = VK_NULL_HANDLE;
    VmaAllocation   readback_alloc       = VK_NULL_HANDLE;
    void*           readback_mapped      = nullptr;
    uint32_t        readback_frame_count = 0;   // frames since last readback
    uint32_t        readback_throttle    = 30;  // readback every N frames
    bool            readback_ready       = false;
    VkDeviceSize    readback_size        = 0;     // capacity actual del buffer
    uint64_t        readback_frame_number = 0;  // which frame the readback data is from (N-1)
    uint8_t         readback_sha256[32]  = {};  // SHA-256 of framebuffer for determinism
    bool            readback_hash_valid  = false; // true if hash is from a valid readback

    bool initReadbackBuffer(VkDeviceSize size);
    void destroyReadbackBuffer();
    void requestReadback(VkCommandBuffer cmd, VkBuffer source_buffer, VkDeviceSize size);
    bool getReadbackData(float* out_tensor, uint32_t max_components);
    uint64_t getReadbackFrameNumber() const { return readback_frame_number; }
    const uint8_t* getReadbackHash() const { return readback_hash_valid ? readback_sha256 : nullptr; }
};

extern VulkanContext vk_ctx;

} // namespace mg
