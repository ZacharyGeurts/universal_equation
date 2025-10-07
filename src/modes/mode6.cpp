#include "render_modes.hpp"
#include "universal_equation.hpp"
#include "dimensional_navigator.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <stdexcept>
#include <cstring>

namespace AMOURANTH {

void renderMode6(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline) {
    // Setup camera for 6D-inspired fractal visualization (recursive patterns, wave-driven motion)
    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f * zoomLevel), aspectRatio, 0.1f, 1000.0f);
    glm::vec3 cameraPos = glm::vec3(sin(wavePhase) * 4.0f, cos(wavePhase) * 4.0f, -15.0f); // Orbital motion
    glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), wavePhase * 0.3f, glm::vec3(1.0f, 1.0f, 1.0f)); // Fractal rotation

    // Get data from UniversalEquation cache for 6D visualization
    if (cache.empty()) {
        throw std::runtime_error("No data in UniversalEquation cache for renderMode6");
    }

    // Use DimensionalNavigator for 6D navigation
    DimensionalNavigator navigator(amouranth->getUniversalEquation());
    navigator.setDimension(6); // Focus on 6D

    // Bind pipeline (ray-traced for RTX support)
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind vertex and index buffers (for fractal geometry)
    VkBuffer vertexBuffers[] = { vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Bind descriptor set for shaders
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = amouranth->getRenderPass();
    renderPassInfo.framebuffer = amouranth->getSwapChainFramebuffers()[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Push constants for shaders (fractal patterns, wave-driven motion)
    struct PushConstants {
        glm::mat4 mvp;
        float beatIntensity;
        float amplitude;
        float time;
        glm::vec3 baseColor;
    } pushConstants;

    // Set values: 6D-inspired fractal, recursive patterns, vibrant visuals
    pushConstants.mvp = proj * view * model;
    pushConstants.beatIntensity = navigator.getInteractionStrength(6);
    pushConstants.amplitude = 1.0f + sin(wavePhase * 3.0f) * 0.5f; // Fast fractal pulsing
    pushConstants.time = wavePhase;
    pushConstants.baseColor = glm::vec3(0.5f, sin(wavePhase * 0.7f), cos(wavePhase * 0.7f)); // Fractal color shift

    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

    // Draw indexed (for fractal geometry)
    uint32_t indexCount = static_cast<uint32_t>(cache.size() * 24); // Indices for recursive fractal
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // Additional elements (recursive fractal layer)
    model = glm::scale(model, glm::vec3(0.5f + sin(wavePhase) * 0.3f)); // Recursive scaling
    pushConstants.mvp = proj * view * model;
    pushConstants.baseColor = glm::vec3(0.5f, cos(wavePhase * 0.7f), sin(wavePhase * 0.7f));
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // End render pass
    vkCmdEndRenderPass(commandBuffer);

    // Error checking
    if (VkResult result = vkEndCommandBuffer(commandBuffer); result != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer for renderMode6");
    }

    // Update vertex buffer if dynamic
    if (!cache.empty()) {
        void* data;
        vkMapMemory(device, vertexBufferMemory, 0, cache.size() * sizeof(UniversalEquation::DimensionData), 0, &data);
        memcpy(data, cache.data(), cache.size() * sizeof(UniversalEquation::DimensionData));
        vkUnmapMemory(device, vertexBufferMemory);
    }
}

} // namespace AMOURANTH