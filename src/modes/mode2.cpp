// mode2.cpp
// Render mode 2 for AMOURANTH RTX Engine
// Zachary Geurts 2025

#include "engine/core.hpp"
#include "Mia.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <stdexcept>
#include <cstring>
#include <span>

void renderMode2(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer,
                 [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] std::span<const UniversalEquation::DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline,
                 float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer) {
    // Initialize Mia for timing and random number generation
    Mia mia(amouranth, amouranth->getLogger());

    // Set 9D simulation mode and get ball data
    amouranth->setCurrentDimension(9); // Use 9D for fractal simulation
    const auto& balls = amouranth->getBalls(); // Access 30,000 balls
    if (balls.empty()) {
        amouranth->getLogger().log(Logging::LogLevel::Error, "RenderMode2", "No ball data for renderMode2",
                                   std::source_location::current());
        throw std::runtime_error("No ball data for renderMode2");
    }

    // Update ball positions (physics simulation)
    amouranth->update(deltaTime); // Use public update method with provided deltaTime

    // Project 9D ball positions onto 2D plane (x-y plane for circular pattern)
    std::vector<float> vertexData;
    vertexData.reserve(balls.size() * 6); // Position (x, y, z) + Normal (x, y, z) per ball
    for (const auto& ball : balls) {
        // Project 9D position to 2D (x, y coordinates, modulated by wavePhase)
        float angle = ball.position.x * 3.14159f + wavePhase * 2.0f; // Angular distribution
        float radius = (ball.position.y + 1.0f) * (0.5f + 0.3f * sin(wavePhase * 3.0f)); // Music-driven radial pulse
        float x = radius * cos(angle);
        float y = radius * sin(angle);
        vertexData.push_back(x); // x position in 2D plane
        vertexData.push_back(y); // y position in 2D plane
        vertexData.push_back(0.0f); // z = 0 for 2D
        vertexData.push_back(0.0f); // Normal x (point-based, simplified)
        vertexData.push_back(0.0f); // Normal y
        vertexData.push_back(1.0f); // Normal z
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
    VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);
    void* indexData;
    vkMapMemory(device, vertexBufferMemory, bufferSize, indexBufferSize, 0, &indexData); // Assume index buffer follows vertex buffer
    memcpy(indexData, indices.data(), indexBufferSize);
    vkUnmapMemory(device, vertexBufferMemory);

    // Bind pipeline (graphics for now, RTX-compatible)
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind vertex and index buffers
    VkBuffer vertexBuffers[] = { vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Bind descriptor set for shaders
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    VkClearValue clearColor = {{{0.1f, 0.1f, 0.2f, 1.0f}}}; // Dark blue background for contrast
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Use Mia for random numbers (spiral fractal with MiaMakesMusic flair)
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
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspectRatio, 0.1f, 1000.0f);
    glm::vec3 cameraPos = glm::vec3(
        cos(wavePhase * 1.2f + randomShift) * 2.0f,
        sin(wavePhase * 1.2f + randomShift) * 2.0f,
        -3.0f + sin(wavePhase * 2.0f + randomShift) * 0.5f
    );
    glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), wavePhase * 0.8f + randomShift, glm::vec3(0.0f, 1.0f, 0.0f));

    pushConstants.mvp = proj * view * model;
    float nurbEnergy = cache.empty() ? 1.0f : static_cast<float>(cache[0].nurbEnergy);
    pushConstants.beatIntensity = nurbEnergy * (1.0f + 0.4f * abs(cos(wavePhase * 3.5f + randomShift)));
    pushConstants.amplitude = 1.0f + cos(wavePhase * 3.0f + randomShift) * 0.7f;
    pushConstants.time = wavePhase;
    pushConstants.baseColor = glm::vec3(
        0.5f + sin(wavePhase * 1.5f + randomShift) * 0.4f,
        0.5f + cos(wavePhase * 1.3f + randomShift) * 0.4f,
        0.7f + sin(wavePhase * 1.7f + randomShift) * 0.2f
    );

    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

    // Draw indexed (for point-based spiral geometry)
    uint32_t indexCount = static_cast<uint32_t>(indices.size()); // One index per ball
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // Additional elements (rotating spiral layer for dynamic effect)
    model = glm::rotate(glm::mat4(1.0f), -wavePhase * 0.8f, glm::vec3(0.0f, 1.0f, 0.0f)); // Counter-rotate
    model = glm::scale(model, glm::vec3(0.8f, 0.8f, 0.8f)); // Slightly smaller scale
    pushConstants.mvp = proj * view * model;
    pushConstants.baseColor = glm::vec3(
        0.7f + cos(wavePhase * 1.5f) * 0.3f,
        0.5f + sin(wavePhase * 1.3f) * 0.3f,
        0.5f + cos(wavePhase * 1.7f) * 0.2f
    );
    pushConstants.amplitude *= 0.85f; // Reduced for second layer
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // End render pass
    vkCmdEndRenderPass(commandBuffer);

    // Error checking
    if (VkResult result = vkEndCommandBuffer(commandBuffer); result != VK_SUCCESS) {
        amouranth->getLogger().log(Logging::LogLevel::Error, "RenderMode2", "Failed to record command buffer for renderMode2: result={}",
                                   std::source_location::current(), result);
        throw std::runtime_error("Failed to record command buffer for renderMode2");
    }
}