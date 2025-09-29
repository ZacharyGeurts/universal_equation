// renderMode6.cpp
// AMOURANTH RTX Engine - Render Mode 6 - September 2025
// Renders 100 orbs (sphere geometry) in 3D, spread to fill screen, modulated by UniversalEquation::EnergyResult for 4th dimension.
// Each orb moves independently with unique rotation and oscillatory translation.
// Uses observable for scale, potential, darkMatter, darkEnergy for position/rotation, and energy values for color.
// Zoomed out 200% by tripling the view translation distance to -9.0f.
// Compatible with 256-byte PushConstants and Vulkan pipeline layout.
// Zachary Geurts, 2025

#include "core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ue_init.hpp"

void renderMode6(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
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

    // Define 100 orbs in a 5x5x4 grid, spread to fill screen
    const int numOrbs = 100;
    const int gridSizeX = 5;
    const int gridSizeY = 5;
    const int gridSizeZ = 4;
    uint32_t indexCount = amouranth->getSphereIndices().size();
    for (int i = 0; i < numOrbs; ++i) {
        // Grid position (x, y, z) to fill screen (-5 to 5 in X/Y, -2 to 2 in Z)
        int x = i % gridSizeX;
        int y = (i / gridSizeX) % gridSizeY;
        int z = i / (gridSizeX * gridSizeY);
        glm::vec3 basePos = glm::vec3(
            (x - (gridSizeX - 1) / 2.0f) * 2.0f, // Spread X over -5 to 5
            (y - (gridSizeY - 1) / 2.0f) * 2.0f, // Spread Y over -5 to 5
            (z - (gridSizeZ - 1) / 2.0f) * 1.0f  // Spread Z over -2 to 2
        );
        // Modulate position with energy values
        glm::vec3 position = basePos + glm::vec3(
            0.2f * static_cast<float>(energyData.potential),
            0.2f * static_cast<float>(energyData.darkMatter),
            0.2f * static_cast<float>(energyData.darkEnergy)
        );
        // Independent oscillatory motion
        float phase = wavePhase + i * 0.1f;
        position += glm::vec3(
            0.2f * sin(phase),
            0.2f * cos(phase),
            0.1f * sin(phase + 1.0f)
        );

        // Independent rotation
        float rotationAngle = wavePhase + 0.5f * static_cast<float>(energyData.darkEnergy) + i * 0.05f;

        // Color: Cycle through palette
        glm::vec3 color = glm::vec3(
            0.5f + 0.5f * sin(i * 0.1f),
            0.5f + 0.5f * sin(i * 0.1f + 2.0f),
            0.5f + 0.5f * sin(i * 0.1f + 4.0f)
        );

        // Model matrix
        pushConstants.model = glm::mat4(1.0f);
        pushConstants.model = glm::scale(pushConstants.model, glm::vec3(scale));
        pushConstants.model = glm::rotate(pushConstants.model, rotationAngle, glm::vec3(0.0f, 0.0f, 1.0f)); // Rotate Z
        pushConstants.model = glm::translate(pushConstants.model, position);

        // Pass data to fragment shader
        pushConstants.extra[0] = glm::vec4(
            static_cast<float>(energyData.observable),
            static_cast<float>(energyData.potential),
            static_cast<float>(energyData.darkMatter),
            static_cast<float>(energyData.darkEnergy)
        );
        pushConstants.extra[1] = glm::vec4(rotationAngle, 0.0f, 0.0f, 0.0f); // Rotation
        pushConstants.extra[2] = glm::vec4(color, 1.0f); // Orb color

        // Push constants and draw
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    }
}