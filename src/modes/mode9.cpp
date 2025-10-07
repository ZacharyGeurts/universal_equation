#include "core.hpp"
#include "universal_equation.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <stdexcept>
#include <cstring>

namespace AMOURANTH {

void renderMode9(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline) {
    // Setup camera for 9D-inspired kaleidoscopic fractal with MiaMakesMusic flair (pulsing, vibrant, music-driven)
    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    float musicZoom = zoomLevel * (1.0f + 0.2f * sin(wavePhase * 4.0f)); // Pulsing zoom for music energy
    glm::mat4 proj = glm::perspective(glm::radians(60.0f * musicZoom), aspectRatio, 0.1f, 1000.0f);
    glm::vec3 cameraPos = glm::vec3(
        sin(wavePhase * 0.9f) * 8.0f + cos(wavePhase * 5.0f) * 0.5f, // Wobble for music vibe
        cos(wavePhase * 0.9f) * 8.0f + sin(wavePhase * 5.0f) * 0.5f, // Wobble for music vibe
        -15.0f
    );
    glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), wavePhase * 0.6f, glm::vec3(1.0f, 1.0f, 1.0f)); // Fractal rotation

    // Get data from UniversalEquation cache for 9D visualization
    if (cache.empty()) {
        throw std::runtime_error("No data in UniversalEquation cache for renderMode9");
    }

    // Use DimensionalNavigator for 9D navigation
    DimensionalNavigator navigator(amouranth->getUniversalEquation());
    navigator.setDimension(9); // Focus on 9D

    // Bind pipeline (ray-traced for RTX support)
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind vertex and index buffers (for kaleidoscopic fractal geometry)
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
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}}; // Black background for vibrant colors
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Push constants for shaders (kaleidoscopic fractal with MiaMakesMusic flair)
    struct PushConstants {
        glm::mat4 mvp;
        float beatIntensity;
        float amplitude;
        float time;
        glm::vec3 baseColor;
    } pushConstants;

    // Set values: 9D-inspired fractal with music-driven pulsing, vibrant rainbow colors
    pushConstants.mvp = proj * view * model;
    pushConstants.beatIntensity = navigator.getInteractionStrength(9) * (1.0f + 0.5f * abs(sin(wavePhase * 4.0f))); // Strong rhythmic pulsing
    pushConstants.amplitude = 1.0f + sin(wavePhase * 4.0f) * 0.8f; // Fast, music-driven fractal pulsing
    pushConstants.time = wavePhase;
    pushConstants.baseColor = glm::vec3(
        0.5f + sin(wavePhase * 1.2f) * 0.5f, // Magenta-blue to rainbow shift
        0.5f + cos(wavePhase * 1.2f) * 0.5f,
        0.5f + sin(wavePhase * 1.5f) * 0.3f  // Gold-hued accent
    );

    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

    // Draw indexed (for fractal geometry)
    uint32_t indexCount = static_cast<uint32_t>(cache.size() * 48); // More indices for recursive fractal
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // Additional elements (recursive fractal layer with mirrored, kaleidoscopic effect)
    model = glm::scale(model, glm::vec3(0.5f + sin(wavePhase * 0.5f) * 0.4f)); // Recursive scaling
    model = glm::rotate(model, wavePhase * 0.7f, glm::vec3(1.0f, 0.0f, 1.0f)); // Additional rotation
    model = glm::scale(model, glm::vec3(-1.0f, 1.0f, 1.0f)); // Mirror for kaleidoscopic effect
    pushConstants.mvp = proj * view * model;
    pushConstants.baseColor = glm::vec3(
        0.5f + cos(wavePhase * 1.2f) * 0.5f, // Complementary rainbow shift
        0.5f + sin(wavePhase * 1.2f) * 0.5f,
        0.5f + cos(wavePhase * 1.5f) * 0.3f  // Gold-hued accent
    );
    pushConstants.amplitude *= 0.9f; // Slightly reduced for second layer
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // End render pass
    vkCmdEndRenderPass(commandBuffer);

    // Error checking
    if (VkResult result = vkEndCommandBuffer(commandBuffer); result != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer for renderMode9");
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