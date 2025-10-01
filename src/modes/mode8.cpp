// renderMode8.cpp
// AMOURANTH RTX Engine - Render Mode 8 - September 2025
// Renders 8 ray-traced orbiting spheres with amazeballs RTX effects, modulated by UniversalEquation::EnergyResult.
// Uses observable for scale, darkEnergy for orbit speed, and energy values for color/emission.
// Compatible with 256-byte PushConstants and Vulkan ray tracing pipeline.
// Zachary Geurts, 2025

#include "core.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "ue_init.hpp"

void renderMode8(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 [[maybe_unused]] const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    // Initialize UniversalEquation for dynamic effects
    UniversalEquation equation;
    equation.setCurrentDimension(1); // Base on 1D for consistency
    equation.setInfluence(1.0);
    equation.advanceCycle();
    EnergyResult energyData = equation.compute();

    // Standardized 256-byte PushConstants for ray tracing
    struct PushConstants {
        alignas(16) glm::mat4 model;       // 64 bytes: Model transformation matrix
        alignas(16) glm::mat4 view_proj;   // 64 bytes: Combined view-projection matrix
        alignas(16) glm::vec4 extra[8];    // 128 bytes: Energy values, orbit params, emission
    } pushConstants = {};

    // View-projection matrix: Perspective with zoom out
    float aspectRatio = static_cast<float>(width) / height;
    pushConstants.view_proj = glm::perspective(glm::radians(45.0f), aspectRatio, 0.1f, 100.0f);
    pushConstants.view_proj = glm::translate(pushConstants.view_proj, glm::vec3(0.0f, 0.0f, -9.0f * zoomLevel));

    // Bind vertex and index buffers (for BLAS construction)
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Render 8 orbiting spheres
    static std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
    static std::uniform_real_distribution<float> angleDist(0.0f, 2.0f * 3.1415926535f);
    static std::vector<float> initialAngles(8);
    if (initialAngles[0] == 0.0f) { // Initialize once
        for (int i = 0; i < 8; ++i) {
            initialAngles[i] = angleDist(rng);
        }
    }

    float orbitSpeed = 0.5f + 0.5f * static_cast<float>(energyData.darkEnergy); // Energy-driven speed
    float orbitRadius = 2.0f + 0.5f * static_cast<float>(energyData.observable); // Energy-driven radius

    for (int i = 0; i < 8; ++i) {
        float angle = initialAngles[i] + wavePhase * orbitSpeed + float(i) * 0.7853981634f; // 45° phase offset
        glm::vec3 orbitPos(orbitRadius * cos(angle), orbitRadius * sin(angle), 0.0f);

        pushConstants.model = glm::mat4(1.0f);
        float scale = 0.3f + 0.2f * sin(wavePhase + float(i)); // Pulsating scale
        pushConstants.model = glm::translate(pushConstants.model, orbitPos);
        pushConstants.model = glm::scale(pushConstants.model, glm::vec3(scale));

        // Energy-based properties
        pushConstants.extra[0] = glm::vec4(
            static_cast<float>(energyData.observable),
            static_cast<float>(energyData.potential) * (i + 1) / 8.0f, // Vary per object
            static_cast<float>(energyData.darkMatter) * (i + 1) / 8.0f,
            static_cast<float>(energyData.darkEnergy)
        );
        pushConstants.extra[1] = glm::vec4(wavePhase, float(i) / 8.0f, 0.0f, 1.0f + 0.5f * sin(wavePhase)); // Phase, index, emission

        // Push constants for current sphere
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_RAYGEN_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT | VK_SHADER_STAGE_ANY_HIT_BIT,
                           0, sizeof(PushConstants), &pushConstants);

        // Ray tracing dispatch for each sphere (simplified; use instancing in practice)
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        vkCmdTraceRaysKHR(commandBuffer, &raygenSBT, &missSBT, &hitSBT, &anyHitSBT, width, height, 1);
    }
}