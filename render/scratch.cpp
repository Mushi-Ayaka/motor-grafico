    vkDestroyShaderModule(vk_ctx.device, shaderModule, nullptr);
    return true;
}

void VulkanSceneData::recordComputeCommandBuffer(VkCommandBuffer cmd, uint32_t width, uint32_t height) {
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
    vkCmdDispatch(cmd, (width + 7) / 8, (height + 7) / 8, 1);
}

void VulkanSceneData::updateUBO(const UboData& ubo) {
