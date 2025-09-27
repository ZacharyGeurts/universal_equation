// File: mode6.cpp (new, based on UniversalEquation math for dimension 6)
// Mode 6: Pulsating sphere for dimension 6, with LOD proxy reducing effective scale for high dims (>6)
// and carrollMod enhancement. Osc includes full asymTerm proxy with sin(pi*phase + osc + vertexFactor).
// Uses simplified 128-byte PushConstants { mat4 model; mat4 view_proj; }

#include "core.hpp"
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

void renderMode6(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {

    // Enhanced local function for dim 6 oscillation, full asymTerm and carrollMod
    auto makeOscValue = [](const DimensionData& cacheEntry, float wavePhase) -> float {
        constexpr float omegaApprox = 0.33f;
        constexpr float weakMod = 0.7f;
        constexpr float meanFieldDamp = 0.8f;
        constexpr float asymCollapse = 0.4f;  // Proxy for asymCollapse_
        constexpr float alphaProxy = 2.0f;  // From exp(-alpha * phase)
        const float twoDMod = std::cos(omegaApprox * 6.0f) * 0.8f;
        const float deMod = static_cast<float>(cacheEntry.darkEnergy) * 0.65f;
        const float dmMod = static_cast<float>(cacheEntry.darkMatter) * 0.45f;
        const float phase = 6.0f / 40.0f;
        const float osc = std::abs(std::cos(2.0f * M_PI * phase));
        const float vertexFactorProxy = 0.5f;  // (nCubeVertices.size() % 10)/10
        const float asymTermProxy = asymCollapse * std::sin(M_PI * phase + osc + vertexFactorProxy) * std::exp(-alphaProxy * phase);
        // carrollMod: 1 - carrollFactor * (1 - 1/maxD * dim), assume carrollFactor=0.5, maxD=20
        const float carrollMod = 1.0f - 0.5f * (1.0f - (6.0f / 20.0f));
        const float oscSin = std::sin(wavePhase + deMod + dmMod + twoDMod + asymTermProxy) * weakMod;
        const float oscCos = std::cos(wavePhase + dmMod);
        const float rawValue = static_cast<float>((cacheEntry.observable * oscSin + cacheEntry.potential * oscCos * twoDMod + asymTermProxy) * meanFieldDamp * carrollMod);
        return rawValue;
    };

    // Find the entry for dimension 6
    const DimensionData* dimData = nullptr;
    for (const auto& entry : cache) {
        if (entry.dimension == 6) {
            dimData = &entry;
            break;
        }
    }

    if (!dimData) {
        std::cerr << "Warning: No data found for dimension 6 in cache.\n";
        return;
    }

    const auto oscValue = makeOscValue(*dimData, wavePhase);

    constexpr float kScaleBias = 0.5f;
    // LOD proxy for d>6: numVertices / (1 << (d/2)), d=6 lodFactor=1<<3=8, reduce scale by 1/lod ~0.125 but mild
    constexpr float lodScaleProxy = 0.8f;  // Mild reduction
    const float avgScaleProxy = 10.0f / (10.0f + 6.0f / 10.0f);
    const float scaleFactor = (1.0f + std::abs(oscValue) * kScaleBias) * avgScaleProxy * lodScaleProxy;

    // Complex rotations with carrollMod in rot speeds
    glm::mat4 model = glm::mat4(1.0f);
    const float carrollMod = 1.0f - 0.5f * (1.0f - 6.0f / 20.0f);
    const float rotY = glm::radians(static_cast<float>(6) * 40.0f * carrollMod + wavePhase * 0.5f);
    const float rotX = glm::radians(static_cast<float>(dimData->potential * 20.0f * carrollMod));
    const float rotZ = glm::radians(static_cast<float>(dimData->darkMatter * 15.0f * carrollMod));
    const float rotW = glm::radians(static_cast<float>(2.0f * wavePhase * carrollMod));
    model = glm::rotate(glm::rotate(glm::rotate(glm::rotate(model, rotW, glm::vec3(1.0f, 1.0f, 0.0f)), rotZ, glm::vec3(0.0f, 0.0f, 1.0f)), rotX, glm::vec3(1.0f, 0.0f, 0.0f)), rotY, glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(scaleFactor * zoomLevel));

    // Dynamic cam for 6D
    glm::vec3 camPos = amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, -40.0f + static_cast<float>(dimData->darkEnergy) * -6.0f);

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

    // Draw the sphere for dimension 6
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
}