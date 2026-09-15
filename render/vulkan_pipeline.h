#pragma once

#include "vulkan_core.h"
#include "scene.h"
#include <vector>

namespace mg {

struct BufferAllocation {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkDeviceSize size = 0;

    void cleanup();
};

struct UboData {
    float camera_pos[3];
    float pad0;
    float camera_target[3];
    float pad1;
    float camera_up[3];
    float fov;
    float time;
    float render_scale;
    uint32_t width;
    uint32_t height;
    float trace_t_max;       // matches GLSL UboData.tmax (std140: offset 64)
    float _pad0;             // std140 struct alignment padding to 80
    float _pad1;
    float _pad2;
};

static_assert(sizeof(UboData) == 80, "UboData must be exactly 80 bytes for std140 layout");

struct VulkanSceneData {
    BufferAllocation bvh_buffer;
    BufferAllocation material_buffer;
    BufferAllocation ubo_buffer;
    BufferAllocation output_buffer;
    BufferAllocation tensor_buffer;      // STORAGE_BUFFER, binding 4
    BufferAllocation tensor_staging[2];  // HOST_VISIBLE | HOST_COHERENT, doble buffer
    BufferAllocation output_staging;     // HOST_VISIBLE, para framebuffer RGBA (determinismo test)
    uint32_t tensor_staging_index = 0;   // alterna 0/1 cada frame
    uint32_t tensor_slot_count = 0;      // N_slots válidos

    VkDescriptorSetLayout descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSet descriptor_set = VK_NULL_HANDLE;

    VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
    VkPipeline compute_pipeline = VK_NULL_HANDLE;
    VkShaderModule shader_module = VK_NULL_HANDLE;

    bool pipeline_ready = false;

    bool init(const OntScene& scene);
    bool initPipeline();
    bool recreateScenePipeline(const std::vector<uint32_t>& spv);
    void updateUBO(const UboData& ubo);
    void recordComputeCommandBuffer(VkCommandBuffer cmd, uint32_t width, uint32_t height);
    bool resizeOutputBuffer(uint32_t width, uint32_t height);
    void cleanup();

    // Tensor buffer readback
    const float* getTensorStagingData() const;
    const float* getOutputStagingData() const;

private:
    bool createDeviceBuffer(const void* data, VkDeviceSize size, BufferAllocation& outBuffer);
    bool createHostBuffer(VkDeviceSize size, BufferAllocation& outBuffer);
};

} // namespace mg
