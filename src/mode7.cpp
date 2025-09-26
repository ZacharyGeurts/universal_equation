#include "core.hpp"
#include <iostream>
#include <vulkan/vulkan.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Mode 7: Rendering with dark matter and dark energy interplay in dimension 7

void renderMode7(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {

    // Local function to compute oscillation value with interplay
    auto makeOscValue = [](const DimensionData& cacheEntry, float wavePhase) -> float {
        const float dmdeMod = static_cast<float>(cacheEntry.darkMatter + cacheEntry.darkEnergy) * 0.7f;
        const float osc = std::sin(wavePhase + dmdeMod) * (1.0f + dmdeMod * 0.3f);
        const float rawValue = static_cast<float>(cacheEntry.observable * osc);
        return rawValue;
    };

    // Check cache integrity
    if (cache.size() < AMOURANTH::kMaxRenderedDimensions) {
        std::cerr << "Warning: Cache size " << cache.size() << " < " << AMOURANTH::kMaxRenderedDimensions << ". Dimensions slacking.\n";
        return;
    }

    // Loop over dimensions, focus on dimension 7
    for (size_t i = 0; i < cache.size(); ++i) {
        if (cache[i].dimension != 7) {
            continue;
        }

        const auto oscValue = makeOscValue(cache[i], wavePhase);

        // Define scale bias constant
        constexpr float kScaleBias = 1.0f;

        const float scaleFactor = 1.0f + static_cast<float>(cache[i].darkMatter + cache[i].darkEnergy) * kScaleBias;

        // Setup transformation matrices with slight wobble
        glm::mat4 model = glm::translate(glm::scale(glm::mat4(1.0f), glm::vec3(scaleFactor * zoomLevel)), glm::vec3(0.0f, std::sin(wavePhase) * 0.2f, 0.0f));

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
        pc.extraData = glm::vec4(1.0f, 0.5f, 0.0f, 0.0f); // Orange tint for interplay

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