// renderMode1.cpp
// AMOURANTH RTX Engine - Render Mode 1 - September 2025
// Renders a sphere flattened to a 2D plane, modulated by UniversalEquation::EnergyResult for 1D math.
// Uses observable for scale, darkEnergy for rotation, and energy values for color.
// Zoomed out 200% by tripling the view translation distance to -9.0f.
// Compatible with 256-byte PushConstants and Vulkan pipeline layout.
// Zachary Geurts, 2025

#include "core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
// Use ue_init.hpp explicitly, avoid universal_equation.hpp
#include "ue_init.hpp"

void renderMode1(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 [[maybe_unused]] const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    // Bind sphere vertex and index buffers
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Initialize UniversalEquation for 1D visualization
    UniversalEquation equation;
    equation.setCurrentDimension(1);
    equation.setInfluence(1.0);
    equation.advanceCycle(); // Update wavePhase for dynamic effects
    EnergyResult energyData = equation.compute();

    // Standardized 256-byte PushConstants
    struct PushConstants {
        alignas(16) glm::mat4 model;       // 64 bytes: Model transformation matrix
        alignas(16) glm::mat4 view_proj;   // 64 bytes: Combined view-projection matrix
        alignas(16) glm::vec4 extra[8];    // 128 bytes: Additional data (energy values, rotation)
    } pushConstants = {};

    // Model matrix: Scale and rotate, flattened to XY plane for 1D visualization
    pushConstants.model = glm::mat4(1.0f);
    float scale = 1.0f + 0.1f * sin(wavePhase) + 0.5f * static_cast<float>(energyData.observable);
    float rotationAngle = wavePhase + 0.5f * static_cast<float>(energyData.darkEnergy); // Rotation driven by darkEnergy
    pushConstants.model = glm::scale(pushConstants.model, glm::vec3(scale, scale, 0.01f)); // Flatten Z for 2D-like effect
    pushConstants.model = glm::rotate(pushConstants.model, rotationAngle, glm::vec3(0.0f, 0.0f, 1.0f)); // Rotate around Z-axis
    pushConstants.model = glm::translate(pushConstants.model, glm::vec3(0.0f, 0.0f, 0.5f * static_cast<float>(energyData.darkEnergy)));

    // View-projection matrix: Perspective with zoom out
    float aspectRatio = static_cast<float>(width) / height;
    pushConstants.view_proj = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
    pushConstants.view_proj = glm::translate(pushConstants.view_proj, glm::vec3(0.0f, 0.0f, -9.0f * zoomLevel));

    // Pass energy values and rotation angle to fragment shader
    pushConstants.extra[0] = glm::vec4(
        static_cast<float>(energyData.observable),
        static_cast<float>(energyData.potential),
        static_cast<float>(energyData.darkMatter),
        static_cast<float>(energyData.darkEnergy)
    );
    pushConstants.extra[1] = glm::vec4(rotationAngle, 0.0f, 0.0f, 0.0f); // Pass rotation for shader effects

    // Push constants to vertex and fragment shaders
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);

    // Draw the sphere (flattened to look like a circle)
    uint32_t indexCount = amouranth->getSphereIndices().size();
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
}