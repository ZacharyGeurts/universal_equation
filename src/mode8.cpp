// File: mode8.cpp (compile to mode8.o)
// Mode 8: Pulsating sphere for dimension 8 with perspective, centered and zoomed out
// Uses simplified 128-byte PushConstants { mat4 model; mat4 view_proj; }

#include "core.hpp"
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <glm/gtc/matrix_transform.hpp>

void renderMode8(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {

    // Local function to compute oscillation value based on cache entry
    auto makeOscValue = [](const DimensionData& cacheEntry, float wavePhase) -> float {
        const float deMod = static_cast<float>(cacheEntry.darkEnergy) * 0.65f;
        const float osc = std::sin(wavePhase + deMod);
        const float rawValue = static_cast<float>(cacheEntry.observable * osc);
        return rawValue;
    };

    // Find the entry for dimension 8
    const DimensionData* dimData = nullptr;
    for (const auto& entry : cache) {
        if (entry.dimension == 8) {
            dimData = &entry;
            break;
        }
    }

    if (!dimData) {
        std::cerr << "Warning: No data found for dimension 8 in cache.\n";
        return;
    }

    const auto oscValue = makeOscValue(*dimData, wavePhase);

    constexpr float kScaleBias = 0.5f;
    const float scaleFactor = 1.0f + std::abs(oscValue) * kScaleBias;

    // Setup transformation matrices, centered with rotation for perspective
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, glm::radians(static_cast<float>(8) * 40.0f), glm::vec3(0.0f, 1.0f, 0.0f)); // y-rotation
    model = glm::scale(model, glm::vec3(scaleFactor * zoomLevel));

    glm::vec3 camPos = amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, -20.0f); // Zoomed out 100%

    glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);

    proj[1][1] *= -1;

    glm::mat4 viewProj = proj * view;

    // Prepare simplified push constants
    PushConstants pc{};
    pc.model = model;
    pc.view_proj = viewProj;

    // Bind buffers
    VkDeviceSize offsets = 0;
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Push constants to shader
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &pc);

    // Draw the sphere for dimension 8
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
}