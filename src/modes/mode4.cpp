// mode4.cpp
// Render mode 4 for AMOURANTH RTX Engine
// Zachary Geurts 2025

#include "engine/core.hpp"
#include "Mia.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <stdexcept>
#include <cstring>
#include <span>
#include <source_location>

void renderMode4(const AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer,
                 VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height,
                 float wavePhase, std::span<const UniversalEquation::DimensionData> cache,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory,
                 VkPipeline pipeline, [[maybe_unused]] float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer) {
    // Initialize Mia for timing and random number generation
    Mia mia(amouranth, amouranth->getLogger());

    // Get ball data (30,000 balls for fractal simulation)
    const auto& balls = amouranth->getBalls();
    if (balls.empty()) {
        amouranth->getLogger().log(Logging::LogLevel::Error, "RenderMode4", "No ball data for renderMode4",
                                   std::source_location::current());
        throw std::runtime_error("No ball data for renderMode4");
    }

    // Project 9D ball positions onto 2D plane with grid-like pattern
    std::vector<float> vertexData;
    vertexData.reserve(balls.size() * 6); // Position (x, y, z) + Normal (x, y, z) per ball
    for (const auto& ball : balls) {
        // Project to 2D with grid-like snapping effect
        float x = ball.x + sin(wavePhase * 2.0f) * 0.2f; // Slight wave modulation
        float y = ball.y + cos(wavePhase * 2.0f) * 0.2f;
        x = round(x * 10.0f) / 10.0f; // Snap to grid
        y = round(y * 10.0f) / 10.0f; // Snap to grid
        vertexData.push_back(x);
        vertexData.push_back(y);
        vertexData.push_back(0.0f); // z = 0 for 2D projection
        vertexData.push_back(0.0f); // Normal x
        vertexData.push_back(0.0f); // Normal y
        vertexData.push_back(1.0f); // Normal z (arbitrary for points)
    }

    // Update vertex buffer with projected ball positions
    VkDeviceSize bufferSize = vertexData.size() * sizeof(float);
    void* data;
    vkMapMemory(device, vertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertexData.data(), bufferSize);
    vkUnmapMemory(device, vertexBufferMemory);

    // Generate indices for point rendering (each ball as a point)
    std::vector<uint32_t> indices(balls.size());
    for (uint32_t i = 0; i < balls.size(); ++i) {
        indices[i] = i;
    }

    // Note: Index buffer memory is not passed; assuming it's handled elsewhere or same as vertexBufferMemory
    // Update index buffer (using vertexBufferMemory as placeholder, ideally use indexBufferMemory)
    VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);
    void* indexData;
    vkMapMemory(device, vertexBufferMemory, 0, indexBufferSize, 0, &indexData); // TODO: Use indexBufferMemory
    memcpy(indexData, indices.data(), indexBufferSize);
    vkUnmapMemory(device, vertexBufferMemory);

    // Begin render pass
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}}; // Black background for vibrant colors
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

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pInheritanceInfo = nullptr
    };
    if (VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo); result != VK_SUCCESS) {
        amouranth->getLogger().log(Logging::LogLevel::Error, "RenderMode4", "Failed to begin command buffer: result={}",
                                   std::source_location::current(), result);
        throw std::runtime_error("Failed to begin command buffer");
    }

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind pipeline (graphics for now, RTX-compatible)
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind vertex and index buffers
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Bind descriptor set for shaders
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // Setup push constants (grid-like visual style)
    struct PushConstants {
        glm::mat4 mvp;
        float beatIntensity;
        float amplitude;
        float time;
        glm::vec3 baseColor;
    } pushConstants;

    // Setup camera with Mia's random numbers
    float randomShift = static_cast<float>(mia.getRandom()); // Use Mia's physics-driven RNG
    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    float musicZoom = zoomLevel * (1.0f + 0.4f * cos(wavePhase * 2.0f + randomShift)); // Grid zoom effect
    glm::mat4 proj = glm::ortho(-aspectRatio * musicZoom, aspectRatio * musicZoom, -1.0f * musicZoom, 1.0f * musicZoom, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(
        glm::vec3(
            cos(wavePhase * 0.6f + randomShift) * 0.5f, // Subtle grid motion
            sin(wavePhase * 0.6f + randomShift) * 0.5f,
            -5.0f
        ),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), wavePhase * 0.3f + randomShift, glm::vec3(0.0f, 0.0f, 1.0f)); // Gentle rotation

    pushConstants.mvp = proj * view * model;
    float beatIntensity = cache.empty() ? 1.0f : cache[0].value;
    pushConstants.beatIntensity = beatIntensity * (1.0f + 0.5f * abs(cos(wavePhase * 2.0f + randomShift))); // Grid pulsing
    pushConstants.amplitude = 1.0f + cos(wavePhase * 2.0f + randomShift) * 0.5f; // Subtle amplitude
    pushConstants.time = wavePhase;
    pushConstants.baseColor = glm::vec3(
        0.5f + cos(wavePhase * 0.9f + randomShift) * 0.5f, // Grid-like color phasing
        0.5f + sin(wavePhase * 0.9f + randomShift) * 0.5f,
        0.5f + cos(wavePhase * 1.2f + randomShift) * 0.3f
    );

    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

    // Draw indexed (for point-based fractal geometry)
    uint32_t indexCount = static_cast<uint32_t>(indices.size()); // One index per ball
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // Additional elements (shifted grid layer for depth)
    model = glm::translate(glm::mat4(1.0f), glm::vec3(cos(wavePhase * 0.5f) * 0.2f, sin(wavePhase * 0.5f) * 0.2f, 0.0f)); // Grid shift
    pushConstants.mvp = proj * view * model;
    pushConstants.baseColor = glm::vec3(
        0.5f + sin(wavePhase * 0.9f) * 0.5f,
        0.5f + cos(wavePhase * 0.9f) * 0.5f,
        0.5f + sin(wavePhase * 1.2f) * 0.3f
    );
    pushConstants.amplitude *= 0.6f; // Reduced for second layer
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // End render pass
    vkCmdEndRenderPass(commandBuffer);

    // Error checking
    if (VkResult result = vkEndCommandBuffer(commandBuffer); result != VK_SUCCESS) {
        amouranth->getLogger().log(Logging::LogLevel::Error, "RenderMode4", "Failed to record command buffer for renderMode4: result={}",
                                   std::source_location::current(), result);
        throw std::runtime_error("Failed to record command buffer for renderMode4");
    }
}