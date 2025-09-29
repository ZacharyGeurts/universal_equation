// renderMode3.cpp
// AMOURANTH RTX Engine - Render Mode 3 - September 2025
// Renders two cubes (approximated from sphere geometry) in 3D, modulated by UniversalEquation::EnergyResult for 3rd dimension.
// First cube uses observable for scale, potential, darkMatter, and darkEnergy for rotation, darkEnergy for translation.
// Second cube uses same scale, synthetic value (observable + potential) for rotation, and offset translation.
// Zoomed out 200% by tripling the view translation distance to -9.0f.
// Compatible with 256-byte PushConstants and Vulkan pipeline layout.
// Zachary Geurts, 2025

#include "core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ue_init.hpp"

void renderMode2(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 [[maybe_unused]] const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    // Bind sphere vertex and index buffers
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Initialize UniversalEquation for 3rd dimension
    UniversalEquation equation;
    equation.setCurrentDimension(3);
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

    // Define two orbs (cubes)
    struct Orb {
        glm::vec3 translate;
        float rotX;
        float rotY;
        float rotZ;
        glm::vec3 color;
    };
    std::vector<Orb> orbs = {
        // Orb 1: Original cube
        {glm::vec3(0.5f * static_cast<float>(energyData.darkEnergy),
                   0.5f * static_cast<float>(energyData.darkEnergy),
                   0.5f * static_cast<float>(energyData.darkEnergy)),
         wavePhase + 0.5f * static_cast<float>(energyData.potential),
         wavePhase + 0.5f * static_cast<float>(energyData.darkMatter),
         wavePhase + 0.5f * static_cast<float>(energyData.darkEnergy),
         glm::vec3(1.0f, 0.2f, 0.2f)}, // Reddish color
        // Orb 2: Additional cube
        {glm::vec3(-1.5f - 0.5f * static_cast<float>(energyData.darkEnergy),
                   -1.5f - 0.5f * static_cast<float>(energyData.darkEnergy),
                   -0.5f * static_cast<float>(energyData.darkEnergy)),
         wavePhase + 0.5f * static_cast<float>(energyData.observable + energyData.potential),
         wavePhase + 0.5f * static_cast<float>(energyData.observable + energyData.potential),
         wavePhase + 0.5f * static_cast<float>(energyData.observable + energyData.potential),
         glm::vec3(0.2f, 1.0f, 0.2f)} // Greenish color
    };

    // Common scale for both orbs to ensure same size
    float scale = 1.0f + 0.1f * sin(wavePhase) + 0.5f * static_cast<float>(energyData.observable);

    // Draw each orb
    uint32_t indexCount = amouranth->getSphereIndices().size();
    for (size_t i = 0; i < orbs.size(); ++i) {
        const Orb& orb = orbs[i];
        // Model matrix: Scale, rotate (X, Y, Z), and translate in 3D
        pushConstants.model = glm::mat4(1.0f);
        pushConstants.model = glm::scale(pushConstants.model, glm::vec3(scale, scale, scale)); // Same scale for 3D
        pushConstants.model = glm::rotate(pushConstants.model, orb.rotX, glm::vec3(1.0f, 0.0f, 0.0f)); // Rotate X
        pushConstants.model = glm::rotate(pushConstants.model, orb.rotY, glm::vec3(0.0f, 1.0f, 0.0f)); // Rotate Y
        pushConstants.model = glm::rotate(pushConstants.model, orb.rotZ, glm::vec3(0.0f, 0.0f, 1.0f)); // Rotate Z
        pushConstants.model = glm::translate(pushConstants.model, orb.translate); // Translate in 3D

        // Pass energy values, rotation angles, and orb color to fragment shader
        pushConstants.extra[0] = glm::vec4(
            static_cast<float>(energyData.observable),
            static_cast<float>(energyData.potential),
            static_cast<float>(energyData.darkMatter),
            static_cast<float>(energyData.darkEnergy)
        );
        pushConstants.extra[1] = glm::vec4(orb.rotX, orb.rotY, orb.rotZ, 0.0f); // Rotation angles
        pushConstants.extra[2] = glm::vec4(orb.color, 1.0f); // Orb-specific color

        // Push constants and draw
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
    }
}