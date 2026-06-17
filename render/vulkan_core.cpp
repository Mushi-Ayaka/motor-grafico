#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vulkan_pipeline.h"
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

    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &image_available_sem) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &render_finished_sem) != VK_SUCCESS ||
        vkCreateFence(device, &fenceInfo, nullptr, &in_flight_fence) != VK_SUCCESS) {
        std::cerr << "Failed to create sync objects!" << std::endl;
        return false;
    }

    return true;
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
        toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toPresent.dstAccessMask = 0;
        vkCmdPipelineBarrier(compute_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &toPresent);
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

        // Transfer to Present Layout
        VkImageMemoryBarrier barrierToPresent = barrierToTransfer;
        barrierToPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrierToPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrierToPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrierToPresent.dstAccessMask = 0;

        vkCmdPipelineBarrier(compute_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrierToPresent);
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

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;
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

} // namespace mg
