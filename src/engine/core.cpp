#include "engine/core.hpp"
#include "engine/Vulkan_init.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <source_location>
#include <random>

void renderMode1(const AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, int width, int height,
                 [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UE::DimensionData> cache,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, [[maybe_unused]] float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer) {
    LOG_DEBUG_CAT("Renderer", "Rendering mode 1 for image index: {}", std::source_location::current(), imageIndex);
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {
            .offset = {0, 0},
            .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}
        },
        .clearValueCount = 1,
        .pClearValues = &clearColor
    };
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    // Will be fixed in the next step
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getBalls().size()), 1, 0, 0, 0);
    vkCmdEndRenderPass(commandBuffer);
}

void renderMode2([[maybe_unused]] const AMOURANTH* amouranth, uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer,
                 [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer,
                 [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height,
                 [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UE::DimensionData> cache,
                 [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory,
                 [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime,
                 [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer) {
    LOG_DEBUG_CAT("Renderer", "Rendering mode 2 (stub) for image index: {}", std::source_location::current(), imageIndex);
    // TODO: Implement rendering logic for mode 2
}

void renderMode3([[maybe_unused]] const AMOURANTH* amouranth, uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer,
                 [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer,
                 [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height,
                 [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UE::DimensionData> cache,
                 [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory,
                 [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime,
                 [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer) {
    LOG_DEBUG_CAT("Renderer", "Rendering mode 3 (stub) for image index: {}", std::source_location::current(), imageIndex);
    // TODO: Implement rendering logic for mode 3
}

void renderMode4([[maybe_unused]] const AMOURANTH* amouranth, uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer,
                 [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer,
                 [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height,
                 [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UE::DimensionData> cache,
                 [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory,
                 [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime,
                 [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer) {
    LOG_DEBUG_CAT("Renderer", "Rendering mode 4 (stub) for image index: {}", std::source_location::current(), imageIndex);
    // TODO: Implement rendering logic for mode 4
}

void renderMode5([[maybe_unused]] const AMOURANTH* amouranth, uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer,
                 [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer,
                 [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height,
                 [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UE::DimensionData> cache,
                 [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory,
                 [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime,
                 [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer) {
    LOG_DEBUG_CAT("Renderer", "Rendering mode 5 (stub) for image index: {}", std::source_location::current(), imageIndex);
    // TODO: Implement rendering logic for mode 5
}

void renderMode6([[maybe_unused]] const AMOURANTH* amouranth, uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer,
                 [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer,
                 [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height,
                 [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UE::DimensionData> cache,
                 [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory,
                 [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime,
                 [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer) {
    LOG_DEBUG_CAT("Renderer", "Rendering mode 6 (stub) for image index: {}", std::source_location::current(), imageIndex);
    // TODO: Implement rendering logic for mode 6
}

void renderMode7([[maybe_unused]] const AMOURANTH* amouranth, uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer,
                 [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer,
                 [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height,
                 [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UE::DimensionData> cache,
                 [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory,
                 [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime,
                 [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer) {
    LOG_DEBUG_CAT("Renderer", "Rendering mode 7 (stub) for image index: {}", std::source_location::current(), imageIndex);
    // TODO: Implement rendering logic for mode 7
}

void renderMode8([[maybe_unused]] const AMOURANTH* amouranth, uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer,
                 [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer,
                 [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height,
                 [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UE::DimensionData> cache,
                 [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory,
                 [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime,
                 [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer) {
    LOG_DEBUG_CAT("Renderer", "Rendering mode 8 (stub) for image index: {}", std::source_location::current(), imageIndex);
    // TODO: Implement rendering logic for mode 8
}

void renderMode9([[maybe_unused]] const AMOURANTH* amouranth, uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer,
                 [[maybe_unused]] VkCommandBuffer commandBuffer, [[maybe_unused]] VkBuffer indexBuffer,
                 [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height,
                 [[maybe_unused]] float wavePhase, [[maybe_unused]] std::span<const UE::DimensionData> cache,
                 [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory,
                 [[maybe_unused]] VkPipeline pipeline, [[maybe_unused]] float deltaTime,
                 [[maybe_unused]] VkRenderPass renderPass, [[maybe_unused]] VkFramebuffer framebuffer) {
    LOG_DEBUG_CAT("Renderer", "Rendering mode 9 (stub) for image index: {}", std::source_location::current(), imageIndex);
    // TODO: Implement rendering logic for mode 9
}