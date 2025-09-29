// renderMode4.cpp
// AMOURANTH RTX Engine - Render Mode 4 - September 2025
// Renders four orbs (sphere geometry) in 3D, modulated by UniversalEquation::EnergyResult for 4th dimension.
// Each orb represents one energy component (observable, potential, darkMatter, darkEnergy).
// Uses energy values for position, scale, and color, with slight rotation for dynamics.
// Zoomed out 200% by tripling the view translation distance to -9.0f.
// Compatible with 256-byte PushConstants and Vulkan pipeline layout.
// Zachary Geurts, 2025

#include "core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ue_init.hpp"

void renderMode4(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
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

    // Define four orbs, each tied to an energy component
    struct Orb {
        glm::vec3 position;
        float scale;
        glm::vec3 color;
    };
    std::vector<Orb> orbs = {
        // Orb 1: Observable
        {glm::vec3(1.0f + 0.5f * static_cast<float>(energyData.observable), 1.0f, 0.0f),
         0.5f + 0.2f * static_cast<float>(energyData.observable), glm::vec3(1.0f, 0.2f, 0.2f)},
        // Orb 2: Potential
        {glm::vec3(-1.0f, 1.0f + 0.5f * static_cast<float>(energyData.potential), 0.0f),
         0.5f + 0.2f * static_cast<float>(energyData.potential), glm::vec3(0.2f, 1.0f, 0.2f)},
        // Orb 3: DarkMatter
        {glm::vec3(1.0f, -1.0f, 0.5f * static_cast<float>(energyData.darkMatter)),
         0.5f + 0.2f * static_cast<float>(energyData.darkMatter), glm::vec3(0.2f, 0.2f, 1.0f)},
        // Orb 4: DarkEnergy
        {glm::vec3(-1.0f, -1.0f, 0.5f * static_cast<float>(energyData.darkEnergy)),
         0.5f + 0.2f * static_cast<float>(energyData.darkEnergy), glm::vec3(1.0f, 1.0f, 0.2f)}
    };

    // Draw each orb
    uint32_t indexCount = amouranth->getSphereIndices().size();
    for (size_t i = 0; i < orbs.size(); ++i) {
        const Orb& orb = orbs[i];
        // Model matrix: Scale, rotate, and translate
        pushConstants.model = glm::mat4(1.0f);
        float rotationAngle = wavePhase + 0.5f * static_cast<float>(energyData.darkEnergy); // Shared rotation for dynamics
        pushConstants.model = glm::scale(pushConstants.model, glm::vec3(orb.scale));
        pushConstants.model = glm::rotate(pushConstants.model, rotationAngle, glm::vec3(0.0f, 0.0f, 1.0f)); // Rotate Z
        pushConstants.model = glm::translate(pushConstants.model, orb.position);

        // Pass energy values and orb-specific data to fragment shader
        pushConstants.extra[0] = glm::vec4(
            static_cast<float>(energyData.observable),
            static_cast<float>(energyData.potential),
            static_cast<float>(energyData.darkMatter),
            static_cast<float>(energyData.darkEnergy)
        );
        pushConstants.extra[1] = glm::vec4(orb.color, 1.0f); // Pass orb color
        pushConstants.extra[2] = glm::vec4(orb.position, rotationAngle); // Pass position and rotation

        // Push constants and draw
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    }
}