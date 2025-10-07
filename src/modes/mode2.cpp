#include "render_modes.hpp"
#include "universal_equation.hpp"
#include "dimensional_navigator.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <stdexcept>
#include <cstring>

namespace AMOURANTH {

void renderMode2(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout,
                 VkDescriptorSet descriptorSet, VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline) {
    // Begin command buffer recording (assuming it's already begun in the caller)
    
    // Setup camera for 2D dynamic rendering (orbiting camera, pulsating colors, spiral pattern)
    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f * zoomLevel), aspectRatio, 0.1f, 1000.0f);
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, -5.0f);
    cameraPos = glm::rotate(cameraPos, wavePhase * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f)); // Orbiting effect
    glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 model = glm::mat4(1.0f); // Base model matrix
    
    // Get data from UniversalEquation cache for 2D visualization
    if (cache.empty()) {
        throw std::runtime_error("No data in UniversalEquation cache for renderMode2");
    }
    
    // Use DimensionalNavigator for 2D navigation (based on your developer focus on multi-dimensional sims)
    DimensionalNavigator navigator(amouranth->getUniversalEquation());
    navigator.setDimension(2); // Focus on 2D
    
    // Bind pipeline (ray-traced for RTX support, tailored to your Vulkan expertise)
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    
    // Bind vertex and index buffers (use provided buffers for spiral or grid visualization)
    VkBuffer vertexBuffers[] = { vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    
    // Bind descriptor set for shaders
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    
    // Begin render pass (using amouranth's render pass)
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
    
    // Push constants for shaders (integrate music-inspired effects: pulsating colors, spiral modulation)
    struct PushConstants {
        glm::mat4 mvp;
        float beatIntensity; // Rhythmic pulsing based on your prefs
        float amplitude;     // Waveform amplitude with spiral twist
        float time;          // For dynamic animation
        glm::vec3 baseColor; // Vibrant, rainbow-shifting colors
    } pushConstants;
    
    // Set values based on your identity: dynamic 2D, vibrant visuals, music-driven, with spiral pattern
    pushConstants.mvp = proj * view * model;
    pushConstants.beatIntensity = navigator.getInteractionStrength(2); // Tie to 2D dimensional data
    pushConstants.amplitude = 1.0f + sin(wavePhase) * 0.5f; // Pulsating scale
    pushConstants.time = wavePhase; // Animation timing
    pushConstants.baseColor = glm::vec3(sin(wavePhase), cos(wavePhase), 0.5f); // Rainbow shift
    
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
    
    // Draw indexed (assuming index count from cache or fixed for grid/spiral)
    uint32_t indexCount = static_cast<uint32_t>(cache.size() * 6); // Example: quad per data point
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    
    // Optional: Render spiral offset elements (dynamic, side-by-side or layered for depth)
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, -1.0f)); // Layered depth
    model = glm::rotate(model, wavePhase, glm::vec3(0.0f, 0.0f, 1.0f)); // Spiral rotation
    pushConstants.mvp = proj * view * model;
    pushConstants.baseColor = glm::vec3(cos(wavePhase), sin(wavePhase), 0.5f); // Complementary color
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    
    // End render pass
    vkCmdEndRenderPass(commandBuffer);
    
    // Error checking (from past: include warnings and handle buffer updates if needed)
    if (VkResult result = vkEndCommandBuffer(commandBuffer); result != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer for renderMode2");
    }
    
    // Optional: Update vertex buffer if dynamic (using provided memory)
    if (!cache.empty()) {
        void* data;
        vkMapMemory(device, vertexBufferMemory, 0, cache.size() * sizeof(UniversalEquation::DimensionData), 0, &data);
        memcpy(data, cache.data(), cache.size() * sizeof(UniversalEquation::DimensionData));
        vkUnmapMemory(device, vertexBufferMemory);
    }
}

} // namespace AMOURANTH