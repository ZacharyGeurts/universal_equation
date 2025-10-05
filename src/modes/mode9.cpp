// src/modes/mode9.cpp
// AMOURANTH RTX Engine - Render Mode 9 - October 2025
// Hypercube-inspired visualization for 9th dimension with 30,000 orbs
// Zachary Geurts, adapted for 9D by Grok, 2025

#include "engine/core.hpp"
#include "ue_init.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <omp.h>

const size_t kNumBalls = 30000; // Scaled up for 9D grandeur

void renderMode9(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 [[maybe_unused]] const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    if (!amouranth || !vertexBuffer || !commandBuffer || !indexBuffer || !pipelineLayout) {
        return;
    }

    // Bind vertex and index buffers
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Initialize UniversalEquation for 9D
    static UniversalEquation equation;
    static bool initialized = false;
    if (!initialized) {
        equation.setCurrentDimension(9); // 9th dimension
        equation.setMode(9);
        equation.setInfluence(3.0f); // Increased influence for wilder dynamics
        equation.setDebug(false);
        equation.initializeCalculator(amouranth);
        equation.initializeBalls(1.5f, 0.08f, kNumBalls); // Smaller orbs, more spread
        initialized = true;
    }

    // Update physics in parallel
    equation.advanceCycle();
#pragma omp parallel
    {
        equation.updateBalls(0.016f); // Fixed timestep for physics
    }

    // Update projected vertices
    auto balls = equation.getBalls();
    std::vector<glm::vec3> updatedVerts(balls.size());
#pragma omp parallel for
    for (size_t i = 0; i < balls.size(); ++i) {
        updatedVerts[i] = (equation.getSimulationTime() >= balls[i].startTime) ? balls[i].position : glm::vec3(0.0f);
    }
    equation.updateProjectedVertices(updatedVerts);

    // Get energy data
    EnergyResult energyData = equation.compute();

    // Push constants for shaders
    struct PushConstants {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 view_proj;
        alignas(16) glm::vec4 extra[8];
    } pushConstants = {};

    // View-projection matrix with 9D-inspired zoom
    float aspectRatio = static_cast<float>(width) / height;
    pushConstants.view_proj = glm::perspective(glm::radians(50.0f), aspectRatio, 0.1f, 150.0f);
    pushConstants.view_proj = glm::translate(pushConstants.view_proj, glm::vec3(0.0f, 0.0f, -12.0f * zoomLevel));

    // Hypercube-inspired scale with oscillation
    float scale = 0.15f + 0.07f * sin(wavePhase * 1.5f) + 0.25f * static_cast<float>(energyData.observable);

    // Draw orbs with 9D flair
    uint32_t indexCount = amouranth->getSphereIndices().size();
    if (indexCount == 0) {
        return;
    }

#pragma omp parallel for
    for (size_t i = 0; i < std::min(balls.size(), kNumBalls); ++i) {
        if (equation.getSimulationTime() < balls[i].startTime) continue;

        // 9D-inspired rotation with quaternions for smoother, chaotic motion
        float rotationAngle = wavePhase * 1.2f + 0.6f * static_cast<float>(energyData.darkEnergy) + i * 0.03f;
        glm::quat rotation = glm::angleAxis(rotationAngle, glm::normalize(glm::vec3(
            sin(i * 0.1f + wavePhase),
            cos(i * 0.15f + wavePhase),
            sin(i * 0.2f + wavePhase)
        )));

        // Psychedelic color palette
        glm::vec3 color = glm::vec3(
            0.6f + 0.4f * sin(i * 0.12f + static_cast<float>(energyData.observable) + wavePhase),
            0.6f + 0.4f * sin(i * 0.14f + static_cast<float>(energyData.potential) + wavePhase * 1.1f),
            0.6f + 0.4f * sin(i * 0.16f + static_cast<float>(energyData.darkMatter) + wavePhase * 1.2f)
        );

        // Model matrix with translation, rotation, and scale
        pushConstants.model = glm::mat4(1.0f);
        pushConstants.model = glm::scale(pushConstants.model, glm::vec3(scale * (1.0f + 0.2f * sin(wavePhase + i * 0.05f))));
        pushConstants.model = pushConstants.model * glm::toMat4(rotation);
        pushConstants.model = glm::translate(pushConstants.model, balls[i].position);

        // Push energy and color data
        pushConstants.extra[0] = glm::vec4(
            static_cast<float>(energyData.observable),
            static_cast<float>(energyData.potential),
            static_cast<float>(energyData.darkMatter),
            static_cast<float>(energyData.darkEnergy)
        );
        pushConstants.extra[1] = glm::vec4(rotationAngle, scale, 0.0f, 0.0f);
        pushConstants.extra[2] = glm::vec4(color, 1.0f);

        // Thread-safe command buffer recording
#pragma omp critical
        {
            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
        }
    }
}