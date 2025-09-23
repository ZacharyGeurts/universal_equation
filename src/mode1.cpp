#include "modes_ue.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // For perspective, lookAt, translate, scale, rotate
#include <vulkan/vulkan.h>
#include <iostream>
#include <cmath>

// Define kMaxRenderedDimensions to avoid private access
static constexpr int kMaxRenderedDimensions = 9;

void renderMode1(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    // Bind vertex and index buffers
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Ensure zoom is always positive and not too small
    float zoomFactor = glm::max(zoomLevel, 0.01f);
    float aspect = static_cast<float>(width) / glm::max(1.0f, static_cast<float>(height));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);

    // Camera setup
    glm::vec3 camPos(0.0f, 0.0f, 10.0f * zoomFactor);
    if (amouranth->isUserCamActive()) {
        camPos = amouranth->getUserCamPos();
    }
    glm::vec3 camTarget(0.0f, 0.0f, 0.0f);
    glm::vec3 camUp(0.0f, 1.0f, 0.0f);
    glm::mat4 view = glm::lookAt(camPos, camTarget, camUp);

    // Divine cycle: smoothly oscillates in [0, 1)
    float cycleProgress = std::fmod(wavePhase / (2.0f * kMaxRenderedDimensions), 1.0f);

    if (cache.size() < kMaxRenderedDimensions) {
        std::cerr << "Warning: Cache size " << cache.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    for (size_t i = 0; i < kMaxRenderedDimensions; ++i) {
        if (i > 0) continue; // Only render dimension 1
        if (cache[i].dimension != 1) {
            std::cerr << "Warning: Invalid cache for dimension 1\n";
            continue;
        }

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(100.0f * zoomFactor, 100.0f * zoomFactor, 0.01f));
        model = glm::rotate(model, wavePhase * 0.1f, glm::vec3(0.0f, 1.0f, 0.0f));

        // Oscillation for observable
        float dimValue = 1.0f;
        float osc = 0.8f + 0.2f * cosf(wavePhase + i);
        float value = static_cast<float>(cache[i].observable * osc);
        value = glm::clamp(value, 0.01f, 1.0f);

        // Color with shimmering aura
        glm::vec3 baseColor = glm::vec3(
            1.0f,
            1.0f - 0.25f * sinf(wavePhase * 0.75f + i),
            1.0f - 0.25f * cosf(wavePhase * 0.5f + i)
        );

        PushConstants pushConstants = {
            model,
            view,
            proj,
            baseColor,
            value,
            dimValue,
            wavePhase,
            cycleProgress,
            static_cast<float>(cache[i].darkMatter),
            static_cast<float>(cache[i].darkEnergy)
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getQuadIndices().size()), 1, 0, 0, 0);
    }

    // Interactions for dimension 1
    amouranth->setCurrentDimension(1);
    auto pairs = amouranth->getInteractions();
    if (pairs.empty()) {
        std::cerr << "Warning: No interactions for dimension 1\n";
        glm::mat4 interactionModel = glm::mat4(1.0f);
        interactionModel = glm::translate(interactionModel, glm::vec3(0.0f, 0.0f, 0.0f));
        interactionModel = glm::scale(interactionModel, glm::vec3(50.0f * zoomFactor, 50.0f * zoomFactor, 0.01f));
        glm::vec3 baseColor = glm::vec3(1.0f, 0.96f, 0.92f);
        PushConstants pushConstants = {
            interactionModel,
            view,
            proj,
            baseColor,
            0.5f,
            1.0f,
            wavePhase,
            cycleProgress,
            0.5f,
            0.5f
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getQuadIndices().size()), 1, 0, 0, 0);
    } else {
        for (const auto& pair : pairs) {
            if (amouranth->getMode() > 1) continue;

            // Interaction strength
            float interactionStrength = static_cast<float>(
                amouranth->computeInteraction(pair.vertexIndex, pair.distance) *
                std::exp(-glm::abs(amouranth->getAlpha() * pair.distance)) *
                amouranth->computePermeation(pair.vertexIndex) *
                glm::max(0.0f, static_cast<float>(pair.strength))
            );
            interactionStrength = glm::max(interactionStrength, 0.01f);
            interactionStrength = glm::min(interactionStrength, 2.0f);

            // Offset visual
            float offset = static_cast<float>(pair.distance) * 0.5f * (1.0f + static_cast<float>(pair.strength) * 0.2f);
            float angle = wavePhase + pair.vertexIndex * 2.0f + pair.distance * 0.1f;
            glm::vec3 offsetPos = glm::vec3(
                offset * sinf(angle),
                offset * cosf(angle),
                offset * 0.2f * sinf(angle * 0.5f)
            );
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), offsetPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(50.0f * zoomFactor, 50.0f * zoomFactor, 0.01f));

            // Color for interaction
            glm::vec3 baseColor = glm::vec3(
                1.0f - 0.3f * sinf(angle),
                1.0f - 0.2f * cosf(angle * 1.2f),
                1.0f - 0.1f * sinf(angle * 0.7f)
            );

            PushConstants pushConstants = {
                interactionModel,
                view,
                proj,
                baseColor,
                interactionStrength * (0.7f + 0.3f * cosf(wavePhase + pair.distance)),
                1.0f,
                wavePhase,
                cycleProgress,
                static_cast<float>(pair.strength),
                static_cast<float>(amouranth->computeDarkEnergy(pair.distance))
            };
            vkCmdPushConstants(commandBuffer, pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getQuadIndices().size()), 1, 0, 0, 0);
        }
    }
}