#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vulkan_pipeline.h"
#include "imgui.h"
#include "imgui_impl_vulkan.h"
#include "imgui_impl_win32.h"
#include <iostream>
#include <vector>

namespace mg {

VulkanContext vk_ctx;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {
    std::cerr << "Vulkan Validation Layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

bool VulkanContext::init(HWND hwnd) {
    if (volkInitialize() != VK_SUCCESS) {
        std::cerr << "Failed to initialize Volk." << std::endl;
        return false;
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Motor Grafico Hermetico";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Hermetic Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME
    };

    // Check validation layer support
    std::vector<const char*> layers;
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
    bool validationAvailable = false;
    for (const auto& l : availableLayers) {
        if (strcmp(l.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            validationAvailable = true; break;
        }
    }
    if (validationAvailable) layers.push_back("VK_LAYER_KHRONOS_validation");
    else std::cerr << "[VK] Validation layers NOT available — running without." << std::endl;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();

    VkResult res = vkCreateInstance(&createInfo, nullptr, &instance);
    if (res != VK_SUCCESS) {
        std::cerr << "[VK] vkCreateInstance FAILED, code=" << res << std::endl;
        return false;
    }
    std::cerr << "[VK] Instance created OK" << std::endl;

    volkLoadInstance(instance);

    // Setup debug messenger
    VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
    debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debugInfo.pfnUserCallback = debugCallback;
    
    if (vkCreateDebugUtilsMessengerEXT(instance, &debugInfo, nullptr, &debug_messenger) != VK_SUCCESS) {
        std::cerr << "Failed to set up debug messenger!" << std::endl;
    }

    // Create Surface
    VkWin32SurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.hwnd = hwnd;
    surfaceInfo.hinstance = GetModuleHandle(nullptr);

    if (vkCreateWin32SurfaceKHR(instance, &surfaceInfo, nullptr, &surface) != VK_SUCCESS) {
        std::cerr << "Failed to create window surface!" << std::endl;
        return false;
    }

    // Select Physical Device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) return false;
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    
    physical_device = devices[0]; // Simple selection

    // Find Queue Families
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            compute_family_idx = i;
        }
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_family_idx = i;
        }
    }

    if (compute_family_idx == ~0u || graphics_family_idx == ~0u) {
        std::cerr << "[VK] No compute or graphics queue found! compute=" << compute_family_idx << " graphics=" << graphics_family_idx << std::endl;
        return false;
    }
    std::cerr << "[VK] Queue families: compute=" << compute_family_idx << " graphics=" << graphics_family_idx << std::endl;

    // Create Logical Device
    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    VkDeviceQueueCreateInfo computeQueueInfo{};
    computeQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    computeQueueInfo.queueFamilyIndex = compute_family_idx;
    computeQueueInfo.queueCount = 1;
    computeQueueInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(computeQueueInfo);

    if (graphics_family_idx != compute_family_idx) {
        VkDeviceQueueCreateInfo graphicsQueueInfo{};
        graphicsQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        graphicsQueueInfo.queueFamilyIndex = graphics_family_idx;
        graphicsQueueInfo.queueCount = 1;
        graphicsQueueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(graphicsQueueInfo);
    }

    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkPhysicalDeviceScalarBlockLayoutFeatures scalarLayoutFeatures{};
    scalarLayoutFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES;
    scalarLayoutFeatures.scalarBlockLayout = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &scalarLayoutFeatures;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VkResult devRes = vkCreateDevice(physical_device, &deviceCreateInfo, nullptr, &device);
    if (devRes != VK_SUCCESS) {
        std::cerr << "[VK] vkCreateDevice FAILED code=" << devRes << std::endl;
        return false;
    }
    std::cerr << "[VK] Logical device created OK" << std::endl;

    volkLoadDevice(device);

    vkGetDeviceQueue(device, compute_family_idx, 0, &compute_queue);
    vkGetDeviceQueue(device, graphics_family_idx, 0, &graphics_queue);

    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physical_device;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;

    if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS) {
        std::cerr << "Failed to create VMA allocator!" << std::endl;
        return false;
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = compute_family_idx;

    if (vkCreateCommandPool(device, &poolInfo, nullptr, &command_pool) != VK_SUCCESS) {
        std::cerr << "Failed to create command pool!" << std::endl;
        return false;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = command_pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(device, &allocInfo, &compute_command_buffer) != VK_SUCCESS) {
        std::cerr << "Failed to allocate command buffers!" << std::endl;
        return false;
    }

    std::cerr << "[VK] init() complete — GPU ready" << std::endl;
    return true;
}

bool VulkanContext::initSwapchain(uint32_t width, uint32_t height) {
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &formatCount, formats.data());

    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = format;
            break;
        }
    }

    swapchain_image_format = surfaceFormat.format;

    // Use surface's currentExtent if fixed (not UINT32_MAX), otherwise use requested size
    if (capabilities.currentExtent.width != UINT32_MAX) {
        swapchain_extent = capabilities.currentExtent;
    } else {
        swapchain_extent = { width, height };
    }

    {
        FILE* log_f = fopen("phase6_diag.txt", "a");
        if (log_f) {
            fprintf(log_f, "[SWAPCHAIN] requested=%dx%d, currentExtent=%dx%d -> actual=%dx%d\n",
                    width, height,
                    capabilities.currentExtent.width, capabilities.currentExtent.height,
                    swapchain_extent.width, swapchain_extent.height);
            fclose(log_f);
        }
    }

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = swapchain_extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // We will blit or copy the compute shader result here

    uint32_t queueFamilyIndices[] = {graphics_family_idx, compute_family_idx};
    if (graphics_family_idx != compute_family_idx) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    createInfo.clipped = VK_TRUE;

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
        std::cerr << "Failed to create swapchain!" << std::endl;
        return false;
    }

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    swapchain_images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchain_images.data());

    swapchain_image_views.resize(imageCount);
    for (size_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchain_images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchain_image_format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &swapchain_image_views[i]) != VK_SUCCESS) {
            std::cerr << "Failed to create image views!" << std::endl;
            return false;
        }
    }

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    if (    vkCreateSemaphore(device, &semaphoreInfo, nullptr, &image_available_sem) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &render_finished_sem) != VK_SUCCESS ||
        vkCreateFence(device, &fenceInfo, nullptr, &in_flight_fence) != VK_SUCCESS) {
        std::cerr << "Failed to create sync objects!" << std::endl;
        return false;
    }

    if (imgui_enabled) createImguiFramebuffers();

    return true;
}

bool VulkanContext::initImGui(HWND hwnd) {
    if (ImGui::GetCurrentContext() == nullptr) {
        std::cerr << "[IMGUI] No ImGui context" << std::endl;
        return false;
    }

    // Descriptor pool
    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000},
    };
    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = (uint32_t)(sizeof(pool_sizes) / sizeof(pool_sizes[0]));
    poolInfo.pPoolSizes = pool_sizes;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &imgui_descriptor_pool) != VK_SUCCESS) {
        std::cerr << "[IMGUI] Failed to create descriptor pool" << std::endl;
        return false;
    }

    // Render pass: overlay that LOADs the existing SDF image (no clear)
    VkAttachmentDescription attachment = {};
    attachment.format = swapchain_image_format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference colorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    VkRenderPassCreateInfo rpInfo = {};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &attachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    if (vkCreateRenderPass(device, &rpInfo, nullptr, &imgui_render_pass) != VK_SUCCESS) {
        std::cerr << "[IMGUI] Failed to create render pass" << std::endl;
        return false;
    }

    // Graphics command pool + buffer (ImGui draws on the graphics queue)
    VkCommandPoolCreateInfo gpool = {};
    gpool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    gpool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    gpool.queueFamilyIndex = graphics_family_idx;
    if (vkCreateCommandPool(device, &gpool, nullptr, &imgui_command_pool) != VK_SUCCESS) {
        std::cerr << "[IMGUI] Failed to create command pool" << std::endl;
        return false;
    }
    VkCommandBufferAllocateInfo cbAlloc = {};
    cbAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAlloc.commandPool = imgui_command_pool;
    cbAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAlloc.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device, &cbAlloc, &imgui_command_buffer) != VK_SUCCESS) {
        std::cerr << "[IMGUI] Failed to allocate command buffer" << std::endl;
        return false;
    }

    VkSemaphoreCreateInfo semInfo = {};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    if (vkCreateSemaphore(device, &semInfo, nullptr, &imgui_present_sem) != VK_SUCCESS) {
        std::cerr << "[IMGUI] Failed to create present semaphore" << std::endl;
        return false;
    }

    ImGui_ImplWin32_Init(hwnd);

    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance;
    init_info.PhysicalDevice = physical_device;
    init_info.Device = device;
    init_info.QueueFamily = graphics_family_idx;
    init_info.Queue = graphics_queue;
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imgui_descriptor_pool;
    init_info.RenderPass = imgui_render_pass;
    init_info.MinImageCount = (uint32_t)swapchain_image_views.size();
    init_info.ImageCount = (uint32_t)swapchain_image_views.size();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    if (!ImGui_ImplVulkan_Init(&init_info)) {
        std::cerr << "[IMGUI] ImGui_ImplVulkan_Init FAILED" << std::endl;
        return false;
    }

    // Upload font atlas (backend creates its own command buffer internally)
    ImGui_ImplVulkan_CreateFontsTexture();
    vkQueueWaitIdle(graphics_queue);
    ImGui_ImplVulkan_DestroyFontsTexture();

    createImguiFramebuffers();
    imgui_enabled = true;
    std::cerr << "[IMGUI] init OK" << std::endl;
    return true;
}

void VulkanContext::createImguiFramebuffers() {
    for (auto fb : imgui_framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
    imgui_framebuffers.clear();
    imgui_framebuffers.resize(swapchain_image_views.size());
    for (size_t i = 0; i < swapchain_image_views.size(); i++) {
        VkFramebufferCreateInfo fbInfo = {};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = imgui_render_pass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &swapchain_image_views[i];
        fbInfo.width = swapchain_extent.width;
        fbInfo.height = swapchain_extent.height;
        fbInfo.layers = 1;
        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &imgui_framebuffers[i]) != VK_SUCCESS) {
            std::cerr << "[IMGUI] Failed to create framebuffer" << std::endl;
        }
    }
}

void VulkanContext::shutdownImGui() {
    if (!imgui_enabled) return;
    vkDeviceWaitIdle(device);
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplWin32_Shutdown();
    for (auto fb : imgui_framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
    imgui_framebuffers.clear();
    if (imgui_render_pass)    vkDestroyRenderPass(device, imgui_render_pass, nullptr);
    if (imgui_descriptor_pool) vkDestroyDescriptorPool(device, imgui_descriptor_pool, nullptr);
    if (imgui_command_pool)   vkDestroyCommandPool(device, imgui_command_pool, nullptr);
    if (imgui_present_sem)    vkDestroySemaphore(device, imgui_present_sem, nullptr);
    imgui_render_pass = VK_NULL_HANDLE;
    imgui_descriptor_pool = VK_NULL_HANDLE;
    imgui_command_pool = VK_NULL_HANDLE;
    imgui_command_buffer = VK_NULL_HANDLE;
    imgui_present_sem = VK_NULL_HANDLE;
    imgui_enabled = false;
}

void VulkanContext::cleanupSwapchain() {
    vkDeviceWaitIdle(device);
    if (image_available_sem) vkDestroySemaphore(device, image_available_sem, nullptr);
    if (render_finished_sem) vkDestroySemaphore(device, render_finished_sem, nullptr);
    if (in_flight_fence) vkDestroyFence(device, in_flight_fence, nullptr);

    for (auto imageView : swapchain_image_views) {
        vkDestroyImageView(device, imageView, nullptr);
    }
    swapchain_image_views.clear();
    if (swapchain) {
        vkDestroySwapchainKHR(device, swapchain, nullptr);
        swapchain = VK_NULL_HANDLE;
    }
}

void VulkanContext::cleanup() {
    vkDeviceWaitIdle(device);
    cleanupSwapchain();
    if (command_pool) vkDestroyCommandPool(device, command_pool, nullptr);
    if (allocator) vmaDestroyAllocator(allocator);
    if (device) vkDestroyDevice(device, nullptr);
    if (debug_messenger) vkDestroyDebugUtilsMessengerEXT(instance, debug_messenger, nullptr);
    if (surface) vkDestroySurfaceKHR(instance, surface, nullptr);
    if (instance) vkDestroyInstance(instance, nullptr);
}

bool VulkanContext::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, VkBuffer& buffer, VmaAllocation& allocation) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;

    if (vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr) != VK_SUCCESS) {
        std::cerr << "Failed to create buffer!" << std::endl;
        return false;
    }
    return true;
}

void VulkanContext::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = command_pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(compute_queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(compute_queue);

    vkFreeCommandBuffers(device, command_pool, 1, &commandBuffer);
}

bool VulkanContext::drawFrame(VulkanSceneData& scene_data, uint32_t render_w, uint32_t render_h) {
    if (!scene_data.pipeline_ready) {
        std::cerr << "[VK] drawFrame skipped: pipeline not ready" << std::endl;
        return true; // not an error, just not ready yet
    }
    vkWaitForFences(device, 1, &in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &in_flight_fence);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, image_available_sem, VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        return false;
    }

    vkResetCommandBuffer(compute_command_buffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(compute_command_buffer, &beginInfo);

    if (test_pattern) {
        // TEST MODE: skip compute, fill swapchain with solid red
        VkImageMemoryBarrier toTransfer{};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.image = swapchain_images[imageIndex];
        toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.baseMipLevel = 0;
        toTransfer.subresourceRange.levelCount = 1;
        toTransfer.subresourceRange.baseArrayLayer = 0;
        toTransfer.subresourceRange.layerCount = 1;
        toTransfer.srcAccessMask = 0;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(compute_command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);

        VkClearColorValue red;
        red.float32[0] = 1.0f; red.float32[1] = 0.0f; red.float32[2] = 0.0f; red.float32[3] = 1.0f;
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(compute_command_buffer, swapchain_images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &red, 1, &range);

        VkImageMemoryBarrier toPresent = toTransfer;
        toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toPresent.newLayout = imgui_enabled ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toPresent.dstAccessMask = imgui_enabled ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0;
        vkCmdPipelineBarrier(compute_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            imgui_enabled ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &toPresent);
    } else {
        {
            static bool first = true;
            if (first) {
                FILE* log_f = fopen("phase6_diag.txt", "a");
                if (log_f) {
                    fprintf(log_f, "[DISPATCH] dispatch size: %dx%d, output buffer size: %llu\n",
                            render_w, render_h,
                            (unsigned long long)scene_data.output_buffer.size);
                    fclose(log_f);
                }
                first = false;
            }
        }
        scene_data.recordComputeCommandBuffer(compute_command_buffer, render_w, render_h);

        // Compute Shader to Swapchain Copy barrier
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {render_w, render_h, 1};

        VkImageMemoryBarrier barrierToTransfer{};
        barrierToTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrierToTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrierToTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierToTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierToTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrierToTransfer.image = swapchain_images[imageIndex];
        barrierToTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrierToTransfer.subresourceRange.baseMipLevel = 0;
        barrierToTransfer.subresourceRange.levelCount = 1;
        barrierToTransfer.subresourceRange.baseArrayLayer = 0;
        barrierToTransfer.subresourceRange.layerCount = 1;
        barrierToTransfer.srcAccessMask = 0;
        barrierToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(compute_command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierToTransfer);

        // Make compute shader output visible to buffer→image copy
        VkBufferMemoryBarrier computeToCopyBarrier{};
        computeToCopyBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        computeToCopyBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        computeToCopyBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        computeToCopyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        computeToCopyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        computeToCopyBarrier.buffer = scene_data.output_buffer.buffer;
        computeToCopyBarrier.offset = 0;
        computeToCopyBarrier.size = scene_data.output_buffer.size;
        vkCmdPipelineBarrier(compute_command_buffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 1, &computeToCopyBarrier, 0, nullptr);

        // Copy Buffer to Image
        vkCmdCopyBufferToImage(compute_command_buffer, scene_data.output_buffer.buffer, swapchain_images[imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // T-102: Also copy to viewport image for ImGui::Image
        copyOutputToViewport(compute_command_buffer, scene_data.output_buffer.buffer, render_w, render_h);

        // Transfer to Present / ImGui-Attachment Layout
        VkImageMemoryBarrier barrierToPresent = barrierToTransfer;
        barrierToPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierToPresent.newLayout = imgui_enabled ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrierToPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrierToPresent.dstAccessMask = imgui_enabled ? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT : 0;

        vkCmdPipelineBarrier(compute_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            imgui_enabled ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrierToPresent);
    }

    vkEndCommandBuffer(compute_command_buffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[] = {image_available_sem};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &compute_command_buffer;
    VkSemaphore signalSemaphores[] = {render_finished_sem};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkQueueSubmit(compute_queue, 1, &submitInfo, in_flight_fence);

    VkSemaphore presentWaitSem = render_finished_sem;

    // ImGui overlay pass (graphics queue) draws on top of the SDF image
    if (imgui_enabled && ImGui::GetDrawData() != nullptr) {
        vkResetCommandBuffer(imgui_command_buffer, 0);
        VkCommandBufferBeginInfo gbBegin{};
        gbBegin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(imgui_command_buffer, &gbBegin);

        VkRenderPassBeginInfo rpBegin{};
        rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBegin.renderPass = imgui_render_pass;
        rpBegin.framebuffer = imgui_framebuffers[imageIndex];
        rpBegin.renderArea.offset = {0, 0};
        rpBegin.renderArea.extent = swapchain_extent;
        rpBegin.clearValueCount = 0;
        vkCmdBeginRenderPass(imgui_command_buffer, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), imgui_command_buffer);

        vkCmdEndRenderPass(imgui_command_buffer);

        VkImageMemoryBarrier toPresent{};
        toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toPresent.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toPresent.image = swapchain_images[imageIndex];
        toPresent.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toPresent.subresourceRange.baseMipLevel = 0;
        toPresent.subresourceRange.levelCount = 1;
        toPresent.subresourceRange.baseArrayLayer = 0;
        toPresent.subresourceRange.layerCount = 1;
        toPresent.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        toPresent.dstAccessMask = 0;
        vkCmdPipelineBarrier(imgui_command_buffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &toPresent);

        vkEndCommandBuffer(imgui_command_buffer);

        VkSemaphore gWaitSems[] = {render_finished_sem};
        VkPipelineStageFlags gWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo gSubmit{};
        gSubmit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        gSubmit.waitSemaphoreCount = 1;
        gSubmit.pWaitSemaphores = gWaitSems;
        gSubmit.pWaitDstStageMask = gWaitStages;
        gSubmit.commandBufferCount = 1;
        gSubmit.pCommandBuffers = &imgui_command_buffer;
        VkSemaphore gSigSems[] = {imgui_present_sem};
        gSubmit.signalSemaphoreCount = 1;
        gSubmit.pSignalSemaphores = gSigSems;
        vkQueueSubmit(graphics_queue, 1, &gSubmit, VK_NULL_HANDLE);

        presentWaitSem = imgui_present_sem;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &presentWaitSem;
    VkSwapchainKHR swapchains[] = {swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(graphics_queue, &presentInfo);

    return true;
}

bool VulkanContext::resizeSwapchain(uint32_t width, uint32_t height) {
    vkDeviceWaitIdle(device);
    cleanupSwapchain();
    return initSwapchain(width, height);
}

// ============================================================================
// Viewport Image (T-102) — offscreen VkImage sampled by ImGui::Image
// ============================================================================

bool VulkanContext::initViewportImage(uint32_t w, uint32_t h) {
    if (viewport_image) destroyViewportImage();
    viewport_w = w; viewport_h = h;

    // 1. VkImage (RGBA8, sampled + transfer dst)
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = {w, h, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    if (vmaCreateImage(allocator, &imageInfo, &allocInfo, &viewport_image, &viewport_image_alloc, nullptr) != VK_SUCCESS) {
        std::cerr << "[VK] viewport image alloc failed" << std::endl;
        return false;
    }

    // 2. ImageView
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = viewport_image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    if (vkCreateImageView(device, &viewInfo, nullptr, &viewport_image_view) != VK_SUCCESS) {
        std::cerr << "[VK] viewport image view failed" << std::endl;
        return false;
    }

    // 3. Sampler
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    vkCreateSampler(device, &samplerInfo, nullptr, &viewport_sampler);

    // 4. Descriptor pool + layout + set
    VkDescriptorPoolSize poolSize = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    vkCreateDescriptorPool(device, &poolInfo, nullptr, &viewport_ds_pool);

    VkDescriptorSetLayoutBinding binding{};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &binding;
    vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &viewport_ds_layout);

    VkDescriptorSetAllocateInfo dsAlloc{};
    dsAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsAlloc.descriptorPool = viewport_ds_pool;
    dsAlloc.descriptorSetCount = 1;
    dsAlloc.pSetLayouts = &viewport_ds_layout;
    vkAllocateDescriptorSets(device, &dsAlloc, &viewport_ds);

    // 5. Update descriptor set
    VkDescriptorImageInfo dii{};
    dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    dii.imageView = viewport_image_view;
    dii.sampler = viewport_sampler;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = viewport_ds;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &dii;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    std::cerr << "[VK] Viewport image ready: " << w << "x" << h << std::endl;
    return true;
}

void VulkanContext::destroyViewportImage() {
    if (viewport_ds_pool)    { vkDestroyDescriptorPool(device, viewport_ds_pool, nullptr); viewport_ds_pool = VK_NULL_HANDLE; }
    if (viewport_ds_layout)  { vkDestroyDescriptorSetLayout(device, viewport_ds_layout, nullptr); viewport_ds_layout = VK_NULL_HANDLE; }
    viewport_ds = VK_NULL_HANDLE;
    if (viewport_sampler)    { vkDestroySampler(device, viewport_sampler, nullptr); viewport_sampler = VK_NULL_HANDLE; }
    if (viewport_image_view) { vkDestroyImageView(device, viewport_image_view, nullptr); viewport_image_view = VK_NULL_HANDLE; }
    if (viewport_image)      { vmaDestroyImage(allocator, viewport_image, viewport_image_alloc); viewport_image = VK_NULL_HANDLE; viewport_image_alloc = VK_NULL_HANDLE; }
}

void VulkanContext::copyOutputToViewport(VkCommandBuffer cmd, VkBuffer output_buffer, uint32_t w, uint32_t h) {
    if (!viewport_image || viewport_w != w || viewport_h != h) {
        if (!initViewportImage(w, h)) return;
    }

    VkImageMemoryBarrier toDst{};
    toDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toDst.image = viewport_image;
    toDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    toDst.srcAccessMask = 0;
    toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toDst);

    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {w, h, 1};
    vkCmdCopyBufferToImage(cmd, output_buffer, viewport_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    VkImageMemoryBarrier toRead{};
    toRead.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toRead.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toRead.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toRead.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toRead.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toRead.image = viewport_image;
    toRead.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    toRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toRead);
}

} // namespace mg
