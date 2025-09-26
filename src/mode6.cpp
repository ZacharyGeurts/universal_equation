#include "core.hpp"
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Mode 6: Rendering with complex oscillation in dimension 6

void renderMode6(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {

    // Local function to compute oscillation value with complex modulation
    auto makeOscValue = [](const DimensionData& cacheEntry, float wavePhase) -> float {
        const float mod = static_cast<float>(cacheEntry.observable + cacheEntry.potential) * 0.6f;
        const float osc = std::sin(wavePhase * 1.5f + mod) * std::cos(wavePhase * 0.5f);
        const float rawValue = static_cast<float>(cacheEntry.observable * osc);
        return rawValue;
    };

    // Check cache integrity
    if (cache.size() < AMOURANTH::kMaxRenderedDimensions) {
        std::cerr << "Warning: Cache size " << cache.size() << " < " << AMOURANTH::kMaxRenderedDimensions << ". Dimensions slacking.\n";
        return;
    }

    // Loop over dimensions, focus on dimension 6
    for (size_t i = 0; i < cache.size(); ++i) {
        if (cache[i].dimension != 6) {
            continue;
        }

        const auto oscValue = makeOscValue(cache[i], wavePhase);

        // Define scale bias constant
        constexpr float kScaleBias = 0.8f;

        const float scaleFactor = 1.0f + static_cast<float>(cache[i].observable) * kScaleBias;

        // Setup transformation matrices with rotation
        glm::mat4 model = glm::rotate(glm::scale(glm::mat4(1.0f), glm::vec3(scaleFactor * zoomLevel)), wavePhase, glm::vec3(1.0f, 1.0f, 0.0f));

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
        pc.cycleProgress = 0.0f; // Placeholder
        pc.zoomLevel = zoomLevel;
        pc.observable = oscValue;
        pc.darkMatter = static_cast<float>(cache[i].darkMatter);
        pc.darkEnergy = static_cast<float>(cache[i].darkEnergy);
        pc.extraData = glm::vec4(0.0f, 1.0f, 1.0f, 0.0f); // Cyan tint for complex oscillation

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