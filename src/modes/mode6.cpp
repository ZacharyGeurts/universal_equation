#include "engine/core.hpp"
#include "Mia.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <stdexcept>
#include <cstring>
#include <span>

void renderMode6(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, [[maybe_unused]] VkBuffer vertexBuffer, [[maybe_unused]] VkCommandBuffer commandBuffer,
                 [[maybe_unused]] VkBuffer indexBuffer, [[maybe_unused]] float zoomLevel, [[maybe_unused]] int width, [[maybe_unused]] int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] std::span<const DimensionData> cache, [[maybe_unused]] VkPipelineLayout pipelineLayout, [[maybe_unused]] VkDescriptorSet descriptorSet,
                 [[maybe_unused]] VkDevice device, [[maybe_unused]] VkDeviceMemory vertexBufferMemory, [[maybe_unused]] VkPipeline pipeline,
                 float deltaTime, VkRenderPass renderPass, VkFramebuffer framebuffer) {
    // Initialize Mia for timing and random number generation
    Mia mia(amouranth, amouranth->getLogger());

    // Set 9D simulation mode and get ball data
    amouranth->setCurrentDimension(9);
    const auto& balls = amouranth->getBalls();
    if (balls.empty()) {
        amouranth->getLogger().log(Logging::LogLevel::Error, "No ball data for renderMode6",
                                   std::source_location::current());
        throw std::runtime_error("No ball data for renderMode6");
    }

    // Update ball positions
    amouranth->update(deltaTime);

    // Project 9D ball positions onto 3D wave-like grid
    std::vector<float> vertexData;
    vertexData.reserve(balls.size() * 9); // Position (x, y, z) + Normal (x, y, z) + Color (r, g, b)
    for (const auto& ball : balls) {
        float randomShift = static_cast<float>(mia.getRandom());
        float x = ball.position.x * 3.0f + randomShift * 0.2f;
        float y = ball.position.y * 3.0f + randomShift * 0.2f;
        float z = sin(x * 2.0f + wavePhase * 3.0f + randomShift) * cos(y * 2.0f + wavePhase * 3.0f) * 0.5f;

        vertexData.push_back(x); // Position x
        vertexData.push_back(y); // Position y
        vertexData.push_back(z); // Position z
        vertexData.push_back(0.0f); // Normal x (approximate)
        vertexData.push_back(0.0f); // Normal y
        vertexData.push_back(1.0f); // Normal z
        vertexData.push_back(0.5f + 0.5f * sin(wavePhase * 2.5f + randomShift)); // Color r
        vertexData.push_back(0.5f + 0.5f * cos(wavePhase * 2.5f + randomShift)); // Color g
        vertexData.push_back(0.5f + 0.3f * sin(wavePhase * 3.5f + randomShift)); // Color b
    }

    // Update vertex buffer
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
    vkMapMemory(device, vertexBufferMemory, bufferSize, indexBufferSize, 0, &indexData);
    memcpy(indexData, indices.data(), indexBufferSize);
    vkUnmapMemory(device, vertexBufferMemory);

    // Bind pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

    // Bind vertex and index buffers
    VkBuffer vertexBuffers[] = { vertexBuffer };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Bind descriptor set
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.framebuffer = framebuffer;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Set up push constants
    struct PushConstants {
        glm::mat4 mvp;
        float beatIntensity;
        float amplitude;
        float time;
        glm::vec3 baseColor;
    } pushConstants;

    float randomShift = static_cast<float>(mia.getRandom());
    float aspectRatio = static_cast<float>(width) / static_cast<float>(height);
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspectRatio, 0.1f, 1000.0f);
    glm::vec3 cameraPos = glm::vec3(
        5.0f * sin(wavePhase * 0.4f + randomShift),
        5.0f * cos(wavePhase * 0.4f + randomShift),
        4.0f + sin(wavePhase * 0.9f + randomShift) * 0.7f
    );
    glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 model = glm::rotate(glm::mat4(1.0f), wavePhase * 0.9f + randomShift, glm::vec3(1.0f, 0.0f, 0.0f));

    pushConstants.mvp = proj * view * model;
    float nurbEnergy = cache.empty() ? 1.0f : static_cast<float>(cache[0].nurbEnergy);
    pushConstants.beatIntensity = nurbEnergy * (1.0f + 0.5f * abs(sin(wavePhase * 4.0f + randomShift)));
    pushConstants.amplitude = 1.0f + sin(wavePhase * 3.0f + randomShift) * 0.8f;
    pushConstants.time = wavePhase;
    pushConstants.baseColor = glm::vec3(
        0.5f + 0.5f * sin(wavePhase * 2.0f + randomShift),
        0.5f + 0.5f * cos(wavePhase * 2.0f + randomShift),
        0.5f + 0.3f * sin(wavePhase * 2.5f + randomShift)
    );

    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

    // Draw indexed (point-based grid)
    uint32_t indexCount = static_cast<uint32_t>(indices.size());
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // Kaleidoscopic effect (rotated grid)
    model = glm::rotate(model, glm::radians(90.0f) + wavePhase * 0.5f + static_cast<float>(mia.getRandom()), glm::vec3(0.0f, 0.0f, 1.0f));
    pushConstants.mvp = proj * view * model;
    pushConstants.baseColor = glm::vec3(
        0.5f + 0.5f * cos(wavePhase * 2.0f + randomShift),
        0.5f + 0.5f * sin(wavePhase * 2.0f + randomShift),
        0.5f + 0.3f * cos(wavePhase * 2.5f + randomShift)
    );
    pushConstants.amplitude *= 0.7f;
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);

    // End render pass
    vkCmdEndRenderPass(commandBuffer);

    // Error checking
    if (VkResult result = vkEndCommandBuffer(commandBuffer); result != VK_SUCCESS) {
        amouranth->getLogger().log(Logging::LogLevel::Error, "Failed to record command buffer for renderMode6: result={}",
                                   std::source_location::current(), result);
        throw std::runtime_error("Failed to record command buffer for renderMode6");
    }
}