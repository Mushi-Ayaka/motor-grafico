#include "vulkan_pipeline.h"
#include "glsl_gen.h"
#include <windows.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

namespace mg {

void BufferAllocation::cleanup() {
    if (buffer) {
        vmaDestroyBuffer(vk_ctx.allocator, buffer, allocation);
        buffer = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        size = 0;
    }
}

bool VulkanSceneData::createDeviceBuffer(const void* data, VkDeviceSize size, BufferAllocation& outBuffer) {
    if (size == 0 || !data) return true;

    BufferAllocation staging;
    if (!vk_ctx.createBuffer(size,
                             VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VMA_MEMORY_USAGE_CPU_ONLY,
                             staging.buffer, staging.allocation)) {
        return false;
    }

    void* mappedData;
    vmaMapMemory(vk_ctx.allocator, staging.allocation, &mappedData);
    memcpy(mappedData, data, size);
    vmaUnmapMemory(vk_ctx.allocator, staging.allocation);

    if (!vk_ctx.createBuffer(size,
                             VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                             VMA_MEMORY_USAGE_GPU_ONLY,
                             outBuffer.buffer, outBuffer.allocation)) {
        staging.cleanup();
        return false;
    }
    outBuffer.size = size;

    vk_ctx.copyBuffer(staging.buffer, outBuffer.buffer, size);
    staging.cleanup();

    return true;
}

bool VulkanSceneData::init(const OntScene& scene) {
    if (!scene.header) return false;

    // 1. Create SSBOs using Staging Buffers
    VkDeviceSize bvhSize = scene.header->bvh_count * sizeof(OntBvhNode);
    if (!createDeviceBuffer(scene.bvh_nodes, bvhSize, bvh_buffer)) return false;

    VkDeviceSize matSize = scene.header->material_count * sizeof(OntMaterial);
    if (!createDeviceBuffer(scene.materials, matSize, material_buffer)) return false;

    // 2. Create UBO (CPU_TO_GPU, persistente)
    if (!vk_ctx.createBuffer(sizeof(UboData),
                             VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                             VMA_MEMORY_USAGE_CPU_TO_GPU,
                             ubo_buffer.buffer, ubo_buffer.allocation)) {
        return false;
    }
    ubo_buffer.size = sizeof(UboData);

    // 3. Create Output Buffer
    VkDeviceSize outSize = scene.width * scene.height * 4;
    if (!vk_ctx.createBuffer(outSize,
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VMA_MEMORY_USAGE_GPU_ONLY,
                             output_buffer.buffer, output_buffer.allocation)) {
        return false;
    }
    output_buffer.size = outSize;

    // 4. Create Layout del Descriptor Set (4 bindings: BVH, Materials, UBO, Output)
    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // BVH
        {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // Materials
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}, // UBO
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr}  // Output Pixels
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    if (vkCreateDescriptorSetLayout(vk_ctx.device, &layoutInfo, nullptr, &descriptor_set_layout) != VK_SUCCESS) {
        return false;
    }

    // 5. Create Pool
    std::vector<VkDescriptorPoolSize> poolSizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1}
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1;

    if (vkCreateDescriptorPool(vk_ctx.device, &poolInfo, nullptr, &descriptor_pool) != VK_SUCCESS) {
        return false;
    }

    // 6. Allocate Descriptor Set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptor_pool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptor_set_layout;

    if (vkAllocateDescriptorSets(vk_ctx.device, &allocInfo, &descriptor_set) != VK_SUCCESS) {
        return false;
    }

    // 7. Update Descriptor Set (link buffers)
    VkDescriptorBufferInfo bvhInfo{bvh_buffer.buffer, 0, bvh_buffer.size};
    VkDescriptorBufferInfo matInfo{material_buffer.buffer, 0, material_buffer.size};
    VkDescriptorBufferInfo uboInfo{ubo_buffer.buffer, 0, ubo_buffer.size};
    VkDescriptorBufferInfo outInfo{output_buffer.buffer, 0, output_buffer.size};

    std::vector<VkWriteDescriptorSet> descriptorWrites(4);
    for (int i = 0; i < 4; i++) {
        descriptorWrites[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[i].dstSet = descriptor_set;
        descriptorWrites[i].dstBinding = i;
        descriptorWrites[i].dstArrayElement = 0;
        descriptorWrites[i].descriptorCount = 1;
        descriptorWrites[i].descriptorType = (i == 2) ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    }

    descriptorWrites[0].pBufferInfo = &bvhInfo;
    descriptorWrites[1].pBufferInfo = &matInfo;
    descriptorWrites[2].pBufferInfo = &uboInfo;
    descriptorWrites[3].pBufferInfo = &outInfo;

    vkUpdateDescriptorSets(vk_ctx.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

    return true;
}

// ============================================================================
// Pipeline creation from SPIR-V binary
// ============================================================================
static bool createPipelineFromSpv(VkDevice device, const std::vector<uint32_t>& spv,
                                  VkPipelineLayout pipeline_layout,
                                  VkShaderModule& out_module,
                                  VkPipeline& out_pipeline) {
    VkShaderModuleCreateInfo moduleInfo{};
    moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = spv.size() * sizeof(uint32_t);
    moduleInfo.pCode = spv.data();

    VkShaderModule module;
    if (vkCreateShaderModule(device, &moduleInfo, nullptr, &module) != VK_SUCCESS) {
        std::cerr << "[VK] createPipelineFromSpv: vkCreateShaderModule FAILED" << std::endl;
        return false;
    }

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = module;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo pipeInfo{};
    pipeInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipeInfo.layout = pipeline_layout;
    pipeInfo.stage = stageInfo;

    VkPipeline pipeline;
    if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipeInfo, nullptr, &pipeline) != VK_SUCCESS) {
        std::cerr << "[VK] createPipelineFromSpv: vkCreateComputePipelines FAILED" << std::endl;
        vkDestroyShaderModule(device, module, nullptr);
        return false;
    }

    out_module = module;
    out_pipeline = pipeline;
    return true;
}

// ============================================================================
// Recreate the pipeline for a specific scene (called after scene load)
// ============================================================================
bool VulkanSceneData::recreateScenePipeline(const std::vector<uint32_t>& spv) {
    VkDevice device = vk_ctx.device;
    if (!device) return false;

    // Destroy old pipeline resources
    vkDeviceWaitIdle(device);

    if (compute_pipeline) {
        vkDestroyPipeline(device, compute_pipeline, nullptr);
        compute_pipeline = VK_NULL_HANDLE;
    }
    if (shader_module) {
        vkDestroyShaderModule(device, shader_module, nullptr);
        shader_module = VK_NULL_HANDLE;
    }
    if (pipeline_layout) {
        vkDestroyPipelineLayout(device, pipeline_layout, nullptr);
        pipeline_layout = VK_NULL_HANDLE;
    }

    // Create pipeline layout
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptor_set_layout;

    if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipeline_layout) != VK_SUCCESS) {
        std::cerr << "[VK] recreateScenePipeline: vkCreatePipelineLayout FAILED" << std::endl;
        return false;
    }

    // Create shader module + pipeline from SPIR-V
    if (!createPipelineFromSpv(device, spv, pipeline_layout, shader_module, compute_pipeline)) {
        std::cerr << "[VK] recreateScenePipeline: createPipelineFromSpv FAILED" << std::endl;
        return false;
    }

    pipeline_ready = true;
    std::cerr << "[VK] Scene-specialized pipeline created (" << (spv.size() * 4) << " bytes SPIR-V)" << std::endl;
    return true;
}

// ============================================================================
// Static helper: readSpvFile
// ============================================================================
static std::vector<char> readSpvFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (file.is_open()) {
        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    }
    char exe_path[MAX_PATH];
    GetModuleFileNameA(nullptr, exe_path, MAX_PATH);
    char* last = strrchr(exe_path, '\\');
    if (last) { last++; *last = '\0'; }
    std::string full = std::string(exe_path) + filename;
    std::ifstream file2(full, std::ios::ate | std::ios::binary);
    if (!file2.is_open()) {
        full = std::string(exe_path) + "shaders/ray_march.spv";
        std::ifstream file3(full, std::ios::ate | std::ios::binary);
        if (!file3.is_open()) {
            std::cerr << "[VK] SPIR-V not found: " << filename << " (also tried " << full << ")" << std::endl;
            return {};
        }
        size_t fileSize = (size_t)file3.tellg();
        std::vector<char> buffer(fileSize);
        file3.seekg(0); file3.read(buffer.data(), fileSize); file3.close();
        std::cerr << "[VK] Loaded SPIR-V from: " << full << std::endl;
        return buffer;
    }
    size_t fileSize = (size_t)file2.tellg();
    std::vector<char> buffer(fileSize);
    file2.seekg(0); file2.read(buffer.data(), fileSize); file2.close();
    std::cerr << "[VK] Loaded SPIR-V from: " << full << std::endl;
    return buffer;
}

bool VulkanSceneData::initPipeline() {
    // This is the fallback pipeline from pre-compiled ray_march.spv.
    // With scene-specialized shaders, this is only used if generation fails.
    std::vector<char> shaderCode = readSpvFile("build/shaders/ray_march.spv");
    if (shaderCode.empty()) {
        std::cerr << "[VK] initPipeline FAILED: SPIR-V not found" << std::endl;
        return false;
    }

    // Convert to uint32_t vector for recreateScenePipeline
    std::vector<uint32_t> spv(shaderCode.size() / 4);
    memcpy(spv.data(), shaderCode.data(), shaderCode.size());

    return recreateScenePipeline(spv);
}

void VulkanSceneData::recordComputeCommandBuffer(VkCommandBuffer cmd, uint32_t width, uint32_t height) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
    vkCmdDispatch(cmd, (width + 7) / 8, (height + 7) / 8, 1);
}

void VulkanSceneData::updateUBO(const UboData& ubo) {
    if (!ubo_buffer.allocation) return;
    void* mappedData;
    vmaMapMemory(vk_ctx.allocator, ubo_buffer.allocation, &mappedData);
    memcpy(mappedData, &ubo, sizeof(UboData));
    vmaFlushAllocation(vk_ctx.allocator, ubo_buffer.allocation, 0, VK_WHOLE_SIZE);
    vmaUnmapMemory(vk_ctx.allocator, ubo_buffer.allocation);
}

bool VulkanSceneData::resizeOutputBuffer(uint32_t width, uint32_t height) {
    if (!pipeline_ready) return false;
    {
        FILE* f = fopen("phase6_diag.txt", "a");
        if (f) { fprintf(f, "[BUF] resizeOutputBuffer(%d, %d)\n", width, height); fclose(f); }
    }

    vkDeviceWaitIdle(vk_ctx.device);

    output_buffer.cleanup();

    VkDeviceSize outSize = (VkDeviceSize)width * height * 4;
    if (!vk_ctx.createBuffer(outSize,
                             VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                             VMA_MEMORY_USAGE_GPU_ONLY,
                             output_buffer.buffer, output_buffer.allocation)) {
        return false;
    }
    output_buffer.size = outSize;

    VkDescriptorBufferInfo outInfo{};
    outInfo.buffer = output_buffer.buffer;
    outInfo.offset = 0;
    outInfo.range = output_buffer.size;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptor_set;
    descriptorWrite.dstBinding = 3;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    descriptorWrite.pBufferInfo = &outInfo;

    vkUpdateDescriptorSets(vk_ctx.device, 1, &descriptorWrite, 0, nullptr);

    return true;
}

void VulkanSceneData::cleanup() {
    bvh_buffer.cleanup();
    material_buffer.cleanup();
    ubo_buffer.cleanup();
    output_buffer.cleanup();

    if (compute_pipeline) {
        vkDestroyPipeline(vk_ctx.device, compute_pipeline, nullptr);
        compute_pipeline = VK_NULL_HANDLE;
    }
    if (shader_module) {
        vkDestroyShaderModule(vk_ctx.device, shader_module, nullptr);
        shader_module = VK_NULL_HANDLE;
    }
    if (pipeline_layout) {
        vkDestroyPipelineLayout(vk_ctx.device, pipeline_layout, nullptr);
        pipeline_layout = VK_NULL_HANDLE;
    }

    if (descriptor_pool) {
        vkDestroyDescriptorPool(vk_ctx.device, descriptor_pool, nullptr);
        descriptor_pool = VK_NULL_HANDLE;
    }
    if (descriptor_set_layout) {
        vkDestroyDescriptorSetLayout(vk_ctx.device, descriptor_set_layout, nullptr);
        descriptor_set_layout = VK_NULL_HANDLE;
    }
}

} // namespace mg
