#include "render_modes.hpp"
#include "universal_equation.hpp"
#include "dimensional_navigator.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <stdexcept>
#include <cstring>

namespace AMOURANTH {

void renderMode3(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline) {
    // Setup camera for 1D+1D+3D structure (dynamic, with bouncing and lateral motion)
    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f * zoomLevel), aspectRatio, 0.1f, 1000.0f);
    glm::vec3 cameraPos = glm::vec3(sin(wavePhase) * 2.0f, 0.0f, -10.0f); // Lateral camera motion
    glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 model = glm::mat4(1.0f); // Base model matrix

    // Get data from UniversalEquation cache for 3D visualization
    if (cache.empty()) {
        throw std::runtime_error("No data in UniversalEquation cache for renderMode3");
    }

    // Use DimensionalNavigator for 3D navigation
    DimensionalNavigator navigator(amouranth->getUniversalEquation());
    navigator.setDimension(3); // Focus on 3D with 1D extensions

    // Bind pipeline (ray-traced for RTX support)
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind vertex and index buffers (for lines and sphere)
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

    // Push constants for shaders (rhythmic pulsing, bouncing simulation)
    struct PushConstants {
        glm::mat4 mvp;
        float beatIntensity;
        float amplitude;
        float time;
        glm::vec3 baseColor;
    } pushConstants;

    // Set values: 1D+1D+3D with bouncing ball, vibrant visuals
    pushConstants.mvp = proj * view * model;
    pushConstants.beatIntensity = navigator.getInteractionStrength(3);
    pushConstants.amplitude = 1.0f + abs(sin(wavePhase * 2.0f)) * 0.5f; // Bouncing effect
    pushConstants.time = wavePhase;
    pushConstants.baseColor = glm::vec3(0.0f, sin(wavePhase), cos(wavePhase)); // Shifting colors

    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

    // Draw indexed (for two lines and one sphere)
    uint32_t indexCount = static_cast<uint32_t>(cache.size() * 36); // More indices for 3D sphere
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // Additional elements (bouncing sphere)
    model = glm::translate(model, glm::vec3(0.0f, sin(wavePhase) * 2.0f, 0.0f)); // Bouncing motion
    model = glm::scale(model, glm::vec3(pushConstants.amplitude));
    pushConstants.mvp = proj * view * model;
    pushConstants.baseColor = glm::vec3(cos(wavePhase), 0.0f, sin(wavePhase));
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // End render pass
    vkCmdEndRenderPass(commandBuffer);

    // Error checking
    if (VkResult result = vkEndCommandBuffer(commandBuffer); result != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer for renderMode3");
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