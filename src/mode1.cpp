#include "core.hpp"
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Mode 1: Basic rendering of a pulsating sphere representing observable energy in 1D-9D

void renderMode1(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {

    // Local function to compute oscillation value based on cache entry
    auto makeOscValue = [](const DimensionData& cacheEntry, float wavePhase) -> float {
        const float deMod = static_cast<float>(cacheEntry.darkEnergy) * 0.65f;
        // Additional modulation or computations can be added here if needed
        const float osc = std::sin(wavePhase + deMod);
        const float rawValue = static_cast<float>(cacheEntry.observable * osc);
        // Clamp or normalize if necessary
        return rawValue;
    };

    // Check cache integrity
    if (cache.size() < AMOURANTH::kMaxRenderedDimensions) {
        std::cerr << "Warning: Cache size " << cache.size() << " < " << AMOURANTH::kMaxRenderedDimensions << ". Dimensions slacking.\n";
    }

    // Loop over dimensions
    for (size_t i = 0; i < cache.size(); ++i) {
        // For mode 1, perhaps focus on dimension 1, but process all for multi-dim visualization
        if (cache[i].dimension != 1) {
            // Skip non-primary dimensions or handle differently; for now, process all
            // If needed, add continue; to skip
        }

        const auto oscValue = makeOscValue(cache[i], wavePhase);

        // Define scale bias constant
        constexpr float kScaleBias = 0.5f; // Adjustable bias for scaling

        const float scaleFactor = 1.0f + static_cast<float>(cache[i].observable) * kScaleBias;

        // Setup transformation matrices
        glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(scaleFactor * zoomLevel));

        glm::vec3 camPos = amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, -5.0f);

        glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        glm::mat4 proj = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 100.0f);

        proj[1][1] *= -1; // Vulkan clip space adjustment

        glm::mat4 viewProj = proj * view * model;

        // Prepare push constants
        PushConstants pc{};
        pc.viewProj = viewProj;
        pc.camPos = camPos;
        pc.wavePhase = wavePhase;
        pc.cycleProgress = 0.0f; // Placeholder, adjust if needed
        pc.zoomLevel = zoomLevel;
        pc.observable = oscValue;
        pc.darkMatter = static_cast<float>(cache[i].darkMatter);
        pc.darkEnergy = static_cast<float>(cache[i].darkEnergy);
        pc.extraData = glm::vec4(0.0f); // Placeholder

        // Bind buffers
        VkDeviceSize offsets = 0;
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offsets);
        vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        // Push constants to shader
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_ALL, 0, sizeof(PushConstants), &pc);

        // Draw the sphere
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
    }
}