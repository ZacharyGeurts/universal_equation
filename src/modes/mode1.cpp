// mode1.cpp: Implementation of mode 1 rendering for AMOURANTH RTX Engine.
// Visualizes 30,000 balls in 8-dimensional space, with dynamics driven by UniversalEquation.
// Each ball is rendered as a small sphere, with scale and color modulated by nurbMatter, nurbEnergy, and waveAmplitude.
// Uses Vulkan for instanced rendering and integrates with DimensionalNavigator for view control.
// Dependencies: engine/core.hpp, universal_equation.hpp, Vulkan 1.3+, GLM.
// Zachary Geurts 2025

#include "engine/core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <cmath>
#include <algorithm>

void renderMode1(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const UniversalEquation::DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline) {
    // Log rendering start
    const Logging::Logger& logger = amouranth->logger_;
    logger.log(Logging::LogLevel::Info, "Starting renderMode1 for image index {}", std::source_location::current(), imageIndex);

    // Get energy results and interactions from UniversalEquation
    UniversalEquation::EnergyResult energy = amouranth->getEnergyResult();
    const auto& balls = amouranth->getBalls();
    const auto& interactions = amouranth->getInteractions();
    logger.log(Logging::LogLevel::Debug, "EnergyResult: observable={:.3f}, potential={:.3f}, nurbMatter={:.3f}, nurbEnergy={:.3f}, GodWaveEnergy={:.3f}, balls size={}, interactions size={}",
               std::source_location::current(), energy.observable, energy.potential, energy.nurbMatter, energy.nurbEnergy, energy.GodWaveEnergy, balls.size(), interactions.size());

    // Get sphere geometry for instanced rendering
    auto vertices = amouranth->getSphereVertices();
    auto indices = amouranth->getSphereIndices();
    if (vertices.empty() || indices.empty()) {
        logger.log(Logging::LogLevel::Error, "Sphere geometry is empty: vertices size={}, indices size={}",
                   std::source_location::current(), vertices.size(), indices.size());
        return;
    }
    logger.log(Logging::LogLevel::Debug, "Using sphere geometry: {} vertices, {} indices",
               std::source_location::current(), vertices.size(), indices.size());

    // Prepare instanced vertex data
    struct InstanceData {
        glm::vec3 position;
        float scale;
        glm::vec4 color;
    };
    std::vector<InstanceData> instanceData;
    instanceData.reserve(balls.size());
    float avgObservable = 0.0f;
    for (const auto& data : cache) {
        avgObservable += static_cast<float>(data.observable);
    }
    avgObservable /= std::max(1.0f, static_cast<float>(cache.size()));
    for (size_t i = 0; i < balls.size(); ++i) {
        const auto& ball = balls[i];
        float scale = ball.radius * (1.0f + static_cast<float>(energy.nurbEnergy) * 0.1f);
        float interactionScale = 1.0f;
        float waveAmp = 0.0f;
        if (i < interactions.size()) {
            interactionScale = static_cast<float>(interactions[i].strength) * 0.05f;
            waveAmp = static_cast<float>(interactions[i].waveAmplitude);
        }
        scale *= (1.0f + interactionScale);
        glm::vec4 color(
            0.5f + 0.5f * std::cos(waveAmp + wavePhase),
            0.5f + 0.5f * std::sin(waveAmp + wavePhase),
            static_cast<float>(energy.observable) / 10.0f,
            1.0f
        );
        instanceData.push_back({ball.position, scale, color});
        if (amouranth->getDebug() && i < 10) {
            logger.log(Logging::LogLevel::Debug, "Ball {}: position=({:.3f}, {:.3f}, {:.3f}), scale={:.3f}, color=({:.3f}, {:.3f}, {:.3f}, {:.3f})",
                       std::source_location::current(), i, ball.position.x, ball.position.y, ball.position.z, scale,
                       color.r, color.g, color.b, color.a);
        }
    }

    // Update instance buffer
    VkDeviceSize instanceBufferSize = instanceData.size() * sizeof(InstanceData);
    void* instanceDataPtr;
    vkMapMemory(device, vertexBufferMemory, 0, instanceBufferSize, 0, &instanceDataPtr);
    memcpy(instanceDataPtr, instanceData.data(), instanceBufferSize);
    vkUnmapMemory(device, vertexBufferMemory);
    logger.log(Logging::LogLevel::Debug, "Updated instance buffer with {} instances", std::source_location::current(), instanceData.size());

    // Prepare push constants
    PushConstants pushConstants{};
    pushConstants.model = glm::mat4(1.0f);
    glm::vec3 cameraPos = amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, 15.0f / zoomLevel);
    glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    float aspect = static_cast<float>(width) / height;
    glm::mat4 proj = glm::perspective(glm::radians(60.0f / zoomLevel), aspect, 0.1f, 100.0f);
    pushConstants.view_proj = proj * view;
    pushConstants.extra[0].x = static_cast<float>(energy.observable);
    pushConstants.extra[0].y = static_cast<float>(energy.potential);
    pushConstants.extra[0].z = static_cast<float>(energy.nurbEnergy);
    pushConstants.extra[0].w = wavePhase;
    pushConstants.extra[1].x = static_cast<float>(energy.nurbMatter);
    pushConstants.extra[1].y = avgObservable;
    pushConstants.extra[1].z = static_cast<float>(amouranth->getAlpha());
    pushConstants.extra[1].w = zoomLevel;
    pushConstants.extra[2].x = static_cast<float>(energy.GodWaveEnergy);
    logger.log(Logging::LogLevel::Debug, "PushConstants: observable={:.3f}, potential={:.3f}, nurbEnergy={:.3f}, wavePhase={:.3f}, nurbMatter={:.3f}, avgObservable={:.3f}, GodWaveEnergy={:.3f}",
               std::source_location::current(), pushConstants.extra[0].x, pushConstants.extra[0].y, pushConstants.extra[0].z, pushConstants.extra[0].w,
               pushConstants.extra[1].x, pushConstants.extra[1].y, pushConstants.extra[2].x);

    // Bind pipeline and descriptor sets
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    logger.log(Logging::LogLevel::Debug, "Bound pipeline and descriptor set for rendering", std::source_location::current());

    // Bind vertex and index buffers
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    logger.log(Logging::LogLevel::Debug, "Bound vertex and index buffers", std::source_location::current());

    // Bind instance buffer (assume same buffer for simplicity)
    vkCmdBindVertexBuffers(commandBuffer, 1, 1, &vertexBuffer, offsets);
    logger.log(Logging::LogLevel::Debug, "Bound instance buffer", std::source_location::current());

    // Push constants
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);
    logger.log(Logging::LogLevel::Debug, "Pushed constants for rendering", std::source_location::current());

    // Draw instanced geometry
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), static_cast<uint32_t>(instanceData.size()), 0, 0, 0);
    logger.log(Logging::LogLevel::Debug, "Issued instanced draw command with {} indices, {} instances",
               std::source_location::current(), indices.size(), instanceData.size());

    logger.log(Logging::LogLevel::Info, "Completed renderMode1 for image index {}", std::source_location::current(), imageIndex);
}