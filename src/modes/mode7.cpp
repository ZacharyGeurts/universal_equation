// mode7.cpp
// Render mode 7 for AMOURANTH RTX Engine
// Zachary Geurts 2025

#include "engine/core.hpp"
#include "Mia.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <stdexcept>
#include <cstring>
#include <span>
#include <source_location>

void renderMode7(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer,
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
        amouranth->getLogger().log(Logging::LogLevel::Error, "RenderMode7", "No ball data for renderMode7",
                                   std::source_location::current());
        throw std::runtime_error("No ball data for renderMode7");
    }

    // Update ball positions (physics simulation)
    amouranth->update(deltaTime); // Use public update method with provided deltaTime

    // Project 9D ball positions onto 3D wave-like grid
    std::vector<float> vertexData;
    vertexData.reserve(balls.size() * 6); // Position (x, y, z) + Normal (x, y, z) per ball
    for (const auto& ball : balls) {
        // Map to 2D grid with wave-like z-displacement
        float x = ball.position.x * 2.0f;
        float y = ball.position.y * 2.0f;
        float z = sin(ball.position.x * 3.14159f + wavePhase) * cos(ball.position.y * 3.14159f + wavePhase) * 0.5f;
        float scale = 1.0f + 0.2f * sin(wavePhase * 2.5f); // Dynamic scaling
        vertexData.push_back(x * scale);
        vertexData.push_back(y * scale);
        vertexData.push_back(z * scale);
        vertexData.push_back(0.0f); // Normal (simplified for points)
        vertexData.push_back(0.0f);
        vertexData.push_back(1.0f);
    }

    // Update vertex buffer with projected ball positions
    VkDeviceSize bufferSize = vertexData.size() * sizeof(float);
    void* data;
    vkMapMemory(device, vertexBufferMemory, 0, bufferSize, 0, &data);
    memcpy(data, vertexData.data(), bufferSize);
    vkUnmapMemory(device, vertexBufferMemory);

    // Generate indices for point rendering
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
    VkClearValue clearColor = {{{0.0f, 0.2f, 0.1f, 1.0f}}}; // Dark teal background
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Use Mia for random numbers (wave-like grid fractal with MiaMakesMusic flair)
    struct PushConstants {
        glm::mat4 mvp;
        float beatIntensity;
        float amplitude;
        float time;
        glm::vec3 baseColor;
    } pushConstants;

    // Setup camera with Mia's random numbers
    float randomShift = static_cast<float>(mia.getRandom());
    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    glm::mat4 proj = glm::perspective(glm::radians(70.0f), aspectRatio, 0.1f, 1000.0f);
    glm::vec3 cameraPos = glm::vec3(
        cos(wavePhase * 1.2f + randomShift) * 4.0f,
        sin(wavePhase * 1.2f + randomShift) * 4.0f,
        -3.5f + cos(wavePhase * 2.0f + randomShift) * 1.0f
    );
    glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), wavePhase * 0.8f + randomShift, glm::vec3(0.5f, 1.0f, 0.0f));

    pushConstants.mvp = proj * view * model;
    float nurbEnergy = cache.empty() ? 1.0f : static_cast<float>(cache[0].nurbEnergy);
    pushConstants.beatIntensity = nurbEnergy * (1.0f + 0.4f * abs(cos(wavePhase * 3.3f + randomShift)));
    pushConstants.amplitude = 1.0f + sin(wavePhase * 2.4f + randomShift) * 0.65f;
    pushConstants.time = wavePhase;
    pushConstants.baseColor = glm::vec3(
        0.5f + sin(wavePhase * 1.4f + randomShift) * 0.3f,
        0.6f + cos(wavePhase * 1.2f + randomShift) * 0.3f,
        0.7f + sin(wavePhase * 1.6f + randomShift) * 0.2f
    );

    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

    // Draw indexed (for point-based wave-like grid geometry)
    uint32_t indexCount = static_cast<uint32_t>(indices.size());
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // Additional elements (offset wave-like grid layer)
    model = glm::rotate(glm::mat4(1.0f), -wavePhase * 0.8f, glm::vec3(0.5f, 1.0f, 0.0f));
    model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.2f * sin(wavePhase)));
    pushConstants.mvp = proj * view * model;
    pushConstants.baseColor = glm::vec3(
        0.6f + cos(wavePhase * 1.4f) * 0.2f,
        0.5f + sin(wavePhase * 1.2f) * 0.2f,
        0.7f + cos(wavePhase * 1.6f) * 0.3f
    );
    pushConstants.amplitude *= 0.9f;
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // End render pass
    vkCmdEndRenderPass(commandBuffer);

    // Error checking
    if (VkResult result = vkEndCommandBuffer(commandBuffer); result != VK_SUCCESS) {
        amouranth->getLogger().log(Logging::LogLevel::Error, "RenderMode7", "Failed to record command buffer for renderMode7: result={}",
                                   std::source_location::current(), result);
        throw std::runtime_error("Failed to record command buffer for renderMode7");
    }
}