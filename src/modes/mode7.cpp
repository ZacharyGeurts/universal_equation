// src/modes/mode7.cpp
// AMOURANTH RTX Engine - Render Mode 7 - September 2025
// Renders 1 orb in 3D for dimension 7, with physics-based bouncing and lateral motion, modulated by UniversalEquation::EnergyResult.
// Orb moves with equation-based forces and initial random velocity, bounces at aquarium edges (-5 to 5 X/Y, -2 to 2 Z).
// Uses observable for scale, potential, darkMatter, darkEnergy for velocity/rotation, and energy values for color.
// Zoomed out 200% by tripling view translation to -9.0f.
// Zachary Geurts, 2025

#include "engine/core.hpp"
#include "ue_init.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <omp.h>
#include <stdexcept>
#include <iostream>

const size_t kNumBalls = 10000; // Number of balls for mode 7

void renderMode7(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    std::cout << "[DEBUG] Entering renderMode7\n";

    // Validate Vulkan resources
    if (!amouranth || !vertexBuffer || !commandBuffer || !indexBuffer || !pipelineLayout) {
        std::cerr << "[ERROR] Mode 7: Invalid Vulkan resources\n";
        return;
    }

    // Bind sphere vertex and index buffers
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    std::cout << "[DEBUG] Mode 7: Bound vertex and index buffers\n";

    // Initialize UniversalEquation for dimension 7
    static UniversalEquation equation;
    static bool initialized = false;
    if (!initialized) {
        equation.setCurrentDimension(7);
        equation.setMode(7);
        equation.setInfluence(2.0);
        equation.setDebug(true);
        equation.initializeCalculator(amouranth);
        equation.initializeBalls(1.0f, 0.1f, kNumBalls);
        initialized = true;
        auto balls = equation.getBalls();
        if (balls.size() != kNumBalls) {
            std::cerr << "[ERROR] Mode 7: Initialized " << balls.size() << " balls, expected " << kNumBalls << "\n";
        } else {
            std::cout << "[DEBUG] Mode 7: Confirmed " << balls.size() << " balls initialized\n";
        }
    }

    // Update physics
    std::cout << "[DEBUG] Mode 7: Calling updateBalls\n";
    equation.advanceCycle();
    equation.updateBalls(0.016f);
    std::cout << "[DEBUG] Mode 7: Completed updateBalls\n";

    // Apply boundary collisions (aquarium edges: -5 to 5 X/Y, -2 to 2 Z)
    const glm::vec3 boundsMin(-5.0f, -5.0f, -2.0f);
    const glm::vec3 boundsMax(5.0f, 5.0f, 2.0f);
    auto balls = equation.getBalls();
    for (size_t i = 0; i < balls.size(); ++i) {
        if (equation.getSimulationTime() < balls[i].startTime) continue;
        if (balls[i].position.x < boundsMin.x) {
            balls[i].position.x = boundsMin.x;
            balls[i].velocity.x = -balls[i].velocity.x;
            if (equation.getDebug()) {
                std::cout << "[DEBUG] Mode 7: Ball " << i << " hit X-min boundary\n";
            }
        }
        if (balls[i].position.x > boundsMax.x) {
            balls[i].position.x = boundsMax.x;
            balls[i].velocity.x = -balls[i].velocity.x;
            if (equation.getDebug()) {
                std::cout << "[DEBUG] Mode 7: Ball " << i << " hit X-max boundary\n";
            }
        }
        if (balls[i].position.y < boundsMin.y) {
            balls[i].position.y = boundsMin.y;
            balls[i].velocity.y = -balls[i].velocity.y;
            if (equation.getDebug()) {
                std::cout << "[DEBUG] Mode 7: Ball " << i << " hit Y-min boundary\n";
            }
        }
        if (balls[i].position.y > boundsMax.y) {
            balls[i].position.y = boundsMax.y;
            balls[i].velocity.y = -balls[i].velocity.y;
            if (equation.getDebug()) {
                std::cout << "[DEBUG] Mode 7: Ball " << i << " hit Y-max boundary\n";
            }
        }
        if (balls[i].position.z < boundsMin.z) {
            balls[i].position.z = boundsMin.z;
            balls[i].velocity.z = -balls[i].velocity.z;
            if (equation.getDebug()) {
                std::cout << "[DEBUG] Mode 7: Ball " << i << " hit Z-min boundary\n";
            }
        }
        if (balls[i].position.z > boundsMax.z) {
            balls[i].position.z = boundsMax.z;
            balls[i].velocity.z = -balls[i].velocity.z;
            if (equation.getDebug()) {
                std::cout << "[DEBUG] Mode 7: Ball " << i << " hit Z-max boundary\n";
            }
        }
    }
    std::cout << "[DEBUG] Mode 7: Applied boundary collisions\n";

    // Update projectedVerts_
    {
        std::lock_guard<std::mutex> lock(equation.getPhysicsMutex());
        std::vector<glm::vec3> updatedVerts(balls.size());
        for (size_t i = 0; i < balls.size(); ++i) {
            updatedVerts[i] = (equation.getSimulationTime() >= balls[i].startTime) ? balls[i].position : glm::vec3(0.0f);
        }
        equation.updateProjectedVertices(updatedVerts);
    }
    std::cout << "[DEBUG] Mode 7: Updated projected vertices\n";

    // Get energy data
    EnergyResult energyData = equation.compute();
    std::cout << "[DEBUG] Mode 7: Computed energy data\n";

    // Standardized 256-byte PushConstants
    struct PushConstants {
        alignas(16) glm::mat4 model;       // 64 bytes
        alignas(16) glm::mat4 view_proj;   // 64 bytes
        alignas(16) glm::vec4 extra[8];    // 128 bytes
    } pushConstants = {};

    // View-projection matrix
    float aspectRatio = static_cast<float>(width) / height;
    pushConstants.view_proj = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
    pushConstants.view_proj = glm::translate(pushConstants.view_proj, glm::vec3(0.0f, 0.0f, -9.0f * zoomLevel));
    std::cout << "[DEBUG] Mode 7: Set up view-projection matrix\n";

    // Common scale
    float scale = 0.2f + 0.05f * sin(wavePhase) + 0.2f * static_cast<float>(energyData.observable);

    // Draw orb
    uint32_t indexCount = amouranth->getSphereIndices().size();
    if (indexCount == 0) {
        std::cerr << "[ERROR] Mode 7: Invalid index count for sphere geometry\n";
        return;
    }
    for (size_t i = 0; i < balls.size() && i < kNumBalls; ++i) {
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
        std::cout << "[DEBUG] Mode 7: Pushed constants for ball " << i << "\n";
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
        std::cout << "[DEBUG] Mode 7: Drew ball " << i << "\n";
    }

    // Export data
    if (!cache.empty()) {
        equation.exportToCSV("mode7_output.csv", cache);
        std::cout << "[DEBUG] Mode 7: Exported CSV\n";
    }
    std::cout << "[DEBUG] Exiting renderMode7\n";
}