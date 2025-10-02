// renderMode7.cpp
// AMOURANTH RTX Engine - Render Mode 7 - October 2025
// Renders 100 orbs in a 10x10 grid with Space Invaders-style horizontal group movement and periodic downward steps.
// 10% of original swoop chance for rare Galaga-style dives, no rotation for marching effect.
// Uses UniversalEquation::EnergyResult for color, scale, and motion parameters.
// Zoomed out 200% with view translation at -9.0f.
// Compatible with 256-byte PushConstants and Vulkan pipeline layout.
// Zachary Geurts, 2025

#include "engine/core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ue_init.hpp"
#include <cstdlib>

void renderMode7(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 [[maybe_unused]] const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    // Bind sphere vertex and index buffers
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Initialize UniversalEquation for 4th dimension
    UniversalEquation equation;
    equation.setCurrentDimension(4);
    equation.setInfluence(1.0);
    equation.advanceCycle(); // Update wavePhase for dynamic effects
    EnergyResult energyData = equation.compute();

    // Standardized 256-byte PushConstants
    struct PushConstants {
        alignas(16) glm::mat4 model;       // 64 bytes: Model transformation matrix
        alignas(16) glm::mat4 view_proj;   // 64 bytes: Combined view-projection matrix
        alignas(16) glm::vec4 extra[8];    // 128 bytes: Additional data (energy values, rotation, color)
    } pushConstants = {};

    // View-projection matrix: Perspective with zoom out
    float aspectRatio = static_cast<float>(width) / height;
    pushConstants.view_proj = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
    pushConstants.view_proj = glm::translate(pushConstants.view_proj, glm::vec3(0.0f, 0.0f, -9.0f * zoomLevel));

    // Common scale for all orbs
    float scale = 0.3f + 0.05f * sin(wavePhase) + 0.2f * static_cast<float>(energyData.observable); // Uniform scale

    // Define 100 orbs in a 10x10 grid
    const int numOrbs = 100;
    const int gridSizeX = 10;
    const int gridSizeY = 10;
    uint32_t indexCount = amouranth->getSphereIndices().size();

    // Space Invaders group motion: Horizontal shift with periodic downward steps
    float groupOffsetX = 5.0f * sin(wavePhase * 0.5f); // Horizontal oscillation (-5 to 5)
    float groupOffsetY = -2.0f * fmod(wavePhase * 0.2f, 2.0f); // Gradual downward shift, resets every cycle

    for (int i = 0; i < numOrbs; ++i) {
        // Grid position (x, y) in a 10x10 formation, centered
        int x = i % gridSizeX;
        int y = i / gridSizeX;
        glm::vec3 basePos = glm::vec3(
            (x - (gridSizeX - 1) / 2.0f) * 1.0f, // Spread X over -4.5 to 4.5
            (y - (gridSizeY - 1) / 2.0f) * 1.0f + 3.0f, // Spread Y over -2 to 8 (start higher on screen)
            0.0f // Z at 0 for 2D-like plane
        );

        // Apply Space Invaders group motion
        glm::vec3 position = basePos + glm::vec3(groupOffsetX, groupOffsetY, 0.0f);

        // Galaga-style swooping motion (10% of original chance)
        float swoopChance = static_cast<float>(energyData.darkMatter) * 0.5f;
        if (static_cast<float>(rand()) / RAND_MAX < (0.2f + swoopChance) * 0.1f) { // Reduced to 10% of original
            float swoopPhase = wavePhase + i * 0.2f;
            float swoopAmplitude = 1.0f + 0.5f * static_cast<float>(energyData.darkEnergy);
            // Diving/swooping path: sinusoidal X and parabolic Y dive
            position.x += swoopAmplitude * sin(swoopPhase * 2.0f);
            position.y -= swoopAmplitude * (1.0f - cos(swoopPhase)) * 2.0f; // Dive downward
            position.z += 0.5f * sin(swoopPhase); // Slight Z oscillation for 3D effect
        } else {
            // Non-swooping orbs get slight oscillation
            float phase = wavePhase + i * 0.1f;
            position += glm::vec3(
                0.2f * sin(phase),
                0.2f * cos(phase),
                0.1f * sin(phase + 1.0f)
            );
        }

        // Color: Cycle through palette
        glm::vec3 color = glm::vec3(
            0.5f + 0.5f * sin(i * 0.1f),
            0.5f + 0.5f * sin(i * 0.1f + 2.0f),
            0.5f + 0.5f * sin(i * 0.1f + 4.0f)
        );

        // Model matrix (no rotation)
        pushConstants.model = glm::mat4(1.0f);
        pushConstants.model = glm::scale(pushConstants.model, glm::vec3(scale));
        pushConstants.model = glm::translate(pushConstants.model, position);

        // Pass data to fragment shader
        pushConstants.extra[0] = glm::vec4(
            static_cast<float>(energyData.observable),
            static_cast<float>(energyData.potential),
            static_cast<float>(energyData.darkMatter),
            static_cast<float>(energyData.darkEnergy)
        );
        pushConstants.extra[1] = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // No rotation
        pushConstants.extra[2] = glm::vec4(color, 1.0f); // Orb color

        // Push constants and draw
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    }
}