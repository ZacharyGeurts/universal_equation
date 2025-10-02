// renderMode1.cpp
// AMOURANTH RTX Engine - Render Mode 1 - October 2025
// Renders a single static, stationary sphere in 3D for the first dimension.
// Uses sphere geometry with fixed scale and position, no rotation.
// Compatible with 256-byte PushConstants and Vulkan pipeline layout.
// Tutorial:
// 1. Bind sphere geometry: Reuse vertex and index buffers from renderMode2 for consistency.
// 2. Initialize UniversalEquation: Set to dimension 1 for minimal influence on static rendering.
// 3. Define PushConstants: 256-byte structure with model, view-projection matrices, and extra data.
// 4. Set up perspective projection: Use glm::perspective for 3D view, adjust zoom with translate.
// 5. Configure sphere: Fixed scale (0.5), centered at origin (0, 0, 0), no rotation.
// 6. Pass data to shaders: Energy values and fixed color (light reddish) via PushConstants.
// 7. Draw: Use Vulkan's indexed drawing with sphere indices.
// Zachary Geurts, 2025

#include "engine/core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ue_init.hpp"

void renderMode1(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    // Step 1: Bind sphere vertex and index buffers
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Step 2: Initialize UniversalEquation for 1st dimension
    UniversalEquation equation;
    equation.setCurrentDimension(1);
    equation.setInfluence(1.0);
    EnergyResult energyData = equation.compute();

    // Step 3: Define 256-byte PushConstants
    struct PushConstants {
        alignas(16) glm::mat4 model;       // 64 bytes: Model transformation matrix
        alignas(16) glm::mat4 view_proj;   // 64 bytes: Combined view-projection matrix
        alignas(16) glm::vec4 extra[8];    // 128 bytes: Additional data (energy values, color)
    } pushConstants = {};

    // Step 4: Set up perspective projection for 3D
    float aspectRatio = static_cast<float>(width) / height;
    pushConstants.view_proj = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
    pushConstants.view_proj = glm::translate(pushConstants.view_proj, glm::vec3(0.0f, 0.0f, -3.0f * zoomLevel));

    // Step 5: Configure static sphere (fixed scale, no rotation, centered)
    pushConstants.model = glm::mat4(1.0f);
    pushConstants.model = glm::scale(pushConstants.model, glm::vec3(0.5f, 0.5f, 0.5f));
    pushConstants.model = glm::translate(pushConstants.model, glm::vec3(0.0f, 0.0f, 0.0f));

    // Step 6: Pass energy values and fixed color to fragment shader
    pushConstants.extra[0] = glm::vec4(
        static_cast<float>(energyData.observable),
        static_cast<float>(energyData.potential),
        static_cast<float>(energyData.darkMatter),
        static_cast<float>(energyData.darkEnergy)
    );
    pushConstants.extra[1] = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // No rotation
    pushConstants.extra[2] = glm::vec4(1.0f, 0.5f, 0.5f, 1.0f); // Light reddish color

    // Step 7: Push constants and draw sphere
    uint32_t indexCount = amouranth->getSphereIndices().size();
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
}