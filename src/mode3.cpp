// File: mode3.cpp (new, based on UniversalEquation math for dimension 3)
// Mode 3: Pulsating sphere for dimension 3, with threeDInfluence proxy in modulation and z-rotation from collapse proxy.
// Integrates carrollMod approximation for scale damping in higher dims.
// Uses simplified 128-byte PushConstants { mat4 model; mat4 view_proj; }

#include "core.hpp"
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

void renderMode3(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {

    // Enhanced local function for dim 3 oscillation, incorporating threeDInfluence proxy and carrollMod approx
    // carrollMod ~ 1 - 0.5*(1 - 1/20 * 3) assuming default carrollFactor=0.5, maxD=20
    auto makeOscValue = [](const DimensionData& cacheEntry, float wavePhase) -> float {
        constexpr float omegaApprox = 0.33f;
        constexpr float threeDProxy = 1.2f;  // Proxy addition for threeDInfluence_ term in compute()
        constexpr float carrollModApprox = 1.0f - 0.5f * (1.0f - (3.0f / 20.0f));  // From Carroll limit in compute()
        const float twoDMod = std::cos(omegaApprox * 3.0f) * 0.8f;  // Extended cos for dim3
        const float deMod = static_cast<float>(cacheEntry.darkEnergy) * 0.65f;
        const float dmMod = static_cast<float>(cacheEntry.darkMatter) * 0.45f;
        const float oscSin = std::sin(wavePhase + deMod + dmMod + twoDMod) * threeDProxy;
        const float oscCos = std::cos(wavePhase + dmMod);
        const float rawValue = static_cast<float>((cacheEntry.observable * oscSin + cacheEntry.potential * oscCos * twoDMod) * carrollModApprox);
        return rawValue;
    };

    // Find the entry for dimension 3
    const DimensionData* dimData = nullptr;
    for (const auto& entry : cache) {
        if (entry.dimension == 3) {
            dimData = &entry;
            break;
        }
    }

    if (!dimData) {
        std::cerr << "Warning: No data found for dimension 3 in cache.\n";
        return;
    }

    const auto oscValue = makeOscValue(*dimData, wavePhase);

    constexpr float kScaleBias = 0.5f;
    const float scaleFactor = 1.0f + std::abs(oscValue) * kScaleBias;

    // Triple rotation: y base + x from potential + z from darkMatter (proxy for collapse influence in dim3)
    glm::mat4 model = glm::mat4(1.0f);
    const float rotY = glm::radians(static_cast<float>(3) * 40.0f + wavePhase * 0.5f);
    const float rotX = glm::radians(static_cast<float>(dimData->potential * 20.0f));
    const float rotZ = glm::radians(static_cast<float>(dimData->darkMatter * 15.0f));  // darkMatter for z-rot (interaction proxy)
    model = glm::rotate(glm::rotate(glm::rotate(model, rotZ, glm::vec3(0.0f, 0.0f, 1.0f)), rotX, glm::vec3(1.0f, 0.0f, 0.0f)), rotY, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scaleFactor * zoomLevel));

    // Dynamic cam with enhanced zoom for 3D feel
    glm::vec3 camPos = amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, -25.0f + static_cast<float>(dimData->darkEnergy) * -3.0f);

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

    // Draw the sphere for dimension 3
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
}