// src/modes/mode8.cpp
// AMOURANTH RTX Engine - Render Mode 8 - September 2025
// Optimized for 30,000 orbs in dimension 8 with physics-based motion
// Zachary Geurts, 2025

#include "engine/core.hpp"
#include "ue_init.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <omp.h>

const size_t kNumBalls = 20;

void renderMode8(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 [[maybe_unused]] const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    if (!amouranth || !vertexBuffer || !commandBuffer || !indexBuffer || !pipelineLayout) {
        return;
    }

    // Bind vertex and index buffers
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Initialize UniversalEquation
    static UniversalEquation equation;
    static bool initialized = false;
    if (!initialized) {
        equation.setCurrentDimension(8);
        equation.setMode(8);
        equation.setInfluence(2.5f);
        equation.setDebug(false);
        equation.initializeCalculator(amouranth);
        equation.initializeBalls(1.2f, 0.12f, kNumBalls);
        initialized = true;
    }

    // Update physics (boundary checks handled in updateBalls)
    equation.advanceCycle();
    equation.updateBalls(0.016f);

    // Update projected vertices
    auto balls = equation.getBalls();
    std::vector<glm::vec3> updatedVerts(balls.size()); // Declare updatedVerts
#pragma omp parallel for
    for (size_t i = 0; i < balls.size(); ++i) {
        updatedVerts[i] = (equation.getSimulationTime() >= balls[i].startTime) ? balls[i].position : glm::vec3(0.0f);
    }
    equation.updateProjectedVertices(updatedVerts);

    // Get energy data
    EnergyResult energyData = equation.compute();

    // Push constants
    struct PushConstants {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view_proj;
        alignas(16) glm::vec4 extra[8];
    } pushConstants = {};

    // View-projection matrix
    float aspectRatio = static_cast<float>(width) / height;
    pushConstants.view_proj = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
    pushConstants.view_proj = glm::translate(pushConstants.view_proj, glm::vec3(0.0f, 0.0f, -9.0f * zoomLevel));

    // Common scale
    float scale = 0.2f + 0.05f * sin(wavePhase) + 0.2f * static_cast<float>(energyData.observable);

    // Draw orbs
    uint32_t indexCount = amouranth->getSphereIndices().size();
    if (indexCount == 0) {
        return;
    }
    for (size_t i = 0; i < std::min(balls.size(), kNumBalls); ++i) {
        if (equation.getSimulationTime() < balls[i].startTime) continue;
        float rotationAngle = wavePhase + 0.5f * static_cast<float>(energyData.darkEnergy) + i * 0.05f;
        glm::vec3 color = glm::vec3(
            0.5f + 0.5f * sin(i * 0.1f + static_cast<float>(energyData.observable)),
            0.5f + 0.5f * sin(i * 0.1f + 2.0f + static_cast<float>(energyData.potential)),
            0.5f + 0.5f * sin(i * 0.1f + 4.0f + static_cast<float>(energyData.darkMatter))
        );
        pushConstants.model = glm::mat4(1.0f);
        pushConstants.model = glm::scale(pushConstants.model, glm::vec3(scale));
        pushConstants.model = glm::rotate(pushConstants.model, rotationAngle, glm::vec3(0.0f, 0.0f, 1.0f));
        pushConstants.model = glm::translate(pushConstants.model, balls[i].position);
        pushConstants.extra[0] = glm::vec4(
            static_cast<float>(energyData.observable),
            static_cast<float>(energyData.potential),
            static_cast<float>(energyData.darkMatter),
            static_cast<float>(energyData.darkEnergy)
        );
        pushConstants.extra[1] = glm::vec4(rotationAngle, 0.0f, 0.0f, 0.0f);
        pushConstants.extra[2] = glm::vec4(color, 1.0f);
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    }
}