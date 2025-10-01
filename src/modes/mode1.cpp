// renderMode1.cpp
// AMOURANTH RTX Engine - Render Mode 1 - September 2025
// Renders a ray-traced 3D sphere with RTX amazeballs effects, modulated by UniversalEquation::EnergyResult for 1D math.
// Uses observable for scale, darkEnergy for rotation, and energy values for color/emission.
// Zoomed out 200% by tripling the view translation distance to -9.0f.
// Compatible with 256-byte PushConstants and Vulkan ray tracing pipeline.
// Zachary Geurts, 2025

#include "core.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ue_init.hpp"

void renderMode1(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 [[maybe_unused]] const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    // Initialize UniversalEquation for 1D visualization
    UniversalEquation equation;
    equation.setCurrentDimension(1);
    equation.setInfluence(1.0);
    equation.advanceCycle(); // Update wavePhase for dynamic effects
    EnergyResult energyData = equation.compute();

    // Standardized 256-byte PushConstants for ray tracing
    struct PushConstants {
        alignas(16) glm::mat4 model;       // 64 bytes: Model transformation matrix
        alignas(16) glm::mat4 view_proj;   // 64 bytes: Combined view-projection matrix
        alignas(16) glm::vec4 extra[8];    // 128 bytes: Energy values, rotation, emission
    } pushConstants = {};

    // Model matrix: Dynamic scale and rotation based on energy
    pushConstants.model = glm::mat4(1.0f);
    float scale = 1.0f + 0.5f * static_cast<float>(energyData.observable) + 0.2f * sin(wavePhase * 2.0);
    float rotationAngle = wavePhase + 0.5f * static_cast<float>(energyData.darkEnergy);
    pushConstants.model = glm::scale(pushConstants.model, glm::vec3(scale));
    pushConstants.model = glm::rotate(pushConstants.model, rotationAngle, glm::vec3(0.0f, 1.0f, 0.0f));
    pushConstants.model = glm::translate(pushConstants.model, glm::vec3(0.0f, 0.0f, -2.0f)); // Position in view

    // View-projection matrix: Perspective with zoom out
    float aspectRatio = static_cast<float>(width) / height;
    pushConstants.view_proj = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
    pushConstants.view_proj = glm::translate(pushConstants.view_proj, glm::vec3(0.0f, 0.0f, -9.0f * zoomLevel));

    // Pass energy values and emission to ray tracing shaders
    pushConstants.extra[0] = glm::vec4(
        static_cast<float>(energyData.observable),
        static_cast<float>(energyData.potential),
        static_cast<float>(energyData.darkMatter),
        static_cast<float>(energyData.darkEnergy)
    );
    pushConstants.extra[1] = glm::vec4(rotationAngle, wavePhase, 0.0f, 1.0f); // WavePhase and emission intensity

    // Bind vertex and index buffers (for BLAS construction if needed)
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Push constants for ray tracing
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT | VK_SHADER_STAGE_ANY_HIT_BIT,
                       0, sizeof(PushConstants), &pushConstants);

    // Ray tracing dispatch
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdTraceRaysKHR(commandBuffer, &raygenSBT, &missSBT, &hitSBT, &anyHitSBT, width, height, 1);
}