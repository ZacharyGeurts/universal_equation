// File: mode1.cpp (updated to incorporate additional math from UniversalEquation)
// Mode 1: Enhanced rendering of a pulsating sphere for dimension 1, now using potential, darkMatter in oscillation
// and dynamic camera zoom based on darkEnergy for deeper integration with compute() terms.
// Uses simplified 128-byte PushConstants { mat4 model; mat4 view_proj; }

#include "core.hpp"
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <glm/gtc/matrix_transform.hpp>

void renderMode1(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {

    // Enhanced local function to compute oscillation value based on cache entry, incorporating potential and darkMatter
    auto makeOscValue = [](const DimensionData& cacheEntry, float wavePhase) -> float {
        const float deMod = static_cast<float>(cacheEntry.darkEnergy) * 0.65f;
        const float dmMod = static_cast<float>(cacheEntry.darkMatter) * 0.45f;
        const float oscSin = std::sin(wavePhase + deMod + dmMod);
        const float oscCos = std::cos(wavePhase + dmMod);  // Additional cos term for potential modulation
        const float rawValue = static_cast<float>(cacheEntry.observable * oscSin + cacheEntry.potential * oscCos);
        return rawValue;
    };

    // Find the entry for dimension 1
    const DimensionData* dimData = nullptr;
    for (const auto& entry : cache) {
        if (entry.dimension == 1) {
            dimData = &entry;
            break;
        }
    }

    if (!dimData) {
        std::cerr << "Warning: No data found for dimension 1 in cache.\n";
        return;
    }

    const auto oscValue = makeOscValue(*dimData, wavePhase);

    constexpr float kScaleBias = 0.5f;
    const float scaleFactor = 1.0f + std::abs(oscValue) * kScaleBias;

    // Setup transformation matrices, centered with dynamic y-rotation incorporating wavePhase for pulsation
    glm::mat4 model = glm::mat4(1.0f);
    const float rotY = glm::radians(static_cast<float>(1) * 40.0f + wavePhase * 0.5f);  // Dynamic rotation using wavePhase
    model = glm::rotate(model, rotY, glm::vec3(0.0f, 1.0f, 0.0f));  // y-rotation
    model = glm::scale(model, glm::vec3(scaleFactor * zoomLevel));

    // Dynamic camera position incorporating darkEnergy for zoom variation (from computeDarkEnergy term)
    glm::vec3 camPos = amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, -20.0f + static_cast<float>(dimData->darkEnergy) * -2.0f);

    glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);

    proj[1][1] *= -1; // Vulkan clip space adjustment

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

    // Draw the sphere for dimension 1
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
}