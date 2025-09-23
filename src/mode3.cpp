#include "modes_ue.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <vulkan/vulkan.h>
#include <iostream>
#include <cmath>

// Define kMaxRenderedDimensions to avoid private access
static constexpr int kMaxRenderedDimensions = 9;

void renderMode3(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    // Bind vertex and index buffers (using sphere geometry for 3D)
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Check sphere indices
    if (amouranth->getSphereIndices().empty()) {
        std::cerr << "Warning: Sphere indices empty\n";
        return;
    }

    // Ensure zoom is positive and not too small
    float zoomFactor = glm::max(zoomLevel, 0.01f);
    float aspect = static_cast<float>(width) / glm::max(1.0f, static_cast<float>(height));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);

    // Camera setup with 3D perspective
    glm::vec3 camPos(0.0f, 0.0f, 15.0f * zoomFactor); // Further back for 3D
    if (amouranth->isUserCamActive()) {
        camPos = amouranth->getUserCamPos();
    }
    glm::vec3 camTarget(0.0f, 0.0f, 0.0f);
    glm::vec3 camUp(0.0f, 1.0f, 0.0f);
    glm::mat4 view = glm::lookAt(camPos, camTarget, camUp);

    // Divine cycle
    float cycleProgress = std::fmod(wavePhase / (2.0f * kMaxRenderedDimensions), 1.0f);

    if (cache.size() < kMaxRenderedDimensions) {
        std::cerr << "Warning: Cache size " << cache.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    for (size_t i = 0; i < kMaxRenderedDimensions; ++i) {
        if (cache[i].dimension != static_cast<int>(i + 1)) {
            std::cerr << "Warning: Invalid cache for dimension " << (i + 1) << "\n";
            continue;
        }
        if (cache[i].dimension != 3) continue; // Only render dimension 3

        // Oscillation influenced by dark matter
        float osc = 1.0f + 0.15f * sinf(wavePhase + i) * (1.0f + static_cast<float>(cache[i].darkMatter) * 0.5f);
        float value = static_cast<float>(cache[i].observable * osc);
        value = glm::clamp(value, 0.01f, 2.0f);

        // 3D helix positioning
        float angle = wavePhase + (i + 1) * 2.0f * glm::pi<float>() / kMaxRenderedDimensions;
        float radius = 3.0f * (1.0f + static_cast<float>(cache[i].darkEnergy) * 0.3f);
        glm::vec3 pos = glm::vec3(
            radius * cosf(angle),
            radius * sinf(angle),
            2.0f * static_cast<float>(cache[i].observable) * sinf(wavePhase + cycleProgress) // Use cycleProgress
        );
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, pos);
        model = glm::scale(model, glm::vec3(0.6f * zoomFactor * osc, 0.6f * zoomFactor * osc, 0.6f * zoomFactor * osc));
        model = glm::rotate(model, wavePhase * 0.3f, glm::vec3(0.0f, 0.0f, 1.0f));

        // Color with 3D depth effect
        glm::vec3 baseColor = glm::vec3(
            0.5f + 0.5f * cosf(wavePhase + i * 0.6f),
            0.5f + 0.5f * sinf(wavePhase + i * 0.4f),
            1.0f - 0.2f * sinf(wavePhase * 0.8f + i)
        );

        PushConstants pushConstants = {
            model,
            view,
            proj,
            baseColor,
            value,
            static_cast<float>(i + 1),
            wavePhase,
            cycleProgress,
            static_cast<float>(cache[i].darkMatter),
            static_cast<float>(cache[i].darkEnergy)
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
    }

    // Interactions for dimension 3
    amouranth->setCurrentDimension(3);
    auto pairs = amouranth->getInteractions();
    if (pairs.empty()) {
        std::cerr << "Warning: No interactions for dimension 3\n";
        glm::mat4 interactionModel = glm::mat4(1.0f);
        interactionModel = glm::translate(interactionModel, glm::vec3(0.0f, 0.0f, 0.0f));
        interactionModel = glm::scale(interactionModel, glm::vec3(0.6f * zoomFactor, 0.6f * zoomFactor, 0.6f * zoomFactor));
        glm::vec3 baseColor = glm::vec3(0.9f, 0.85f, 1.0f);
        PushConstants pushConstants = {
            interactionModel,
            view,
            proj,
            baseColor,
            0.5f,
            3.0f,
            wavePhase,
            cycleProgress,
            0.5f,
            0.5f
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
    } else {
        for (const auto& pair : pairs) {
            if (amouranth->getMode() != 3) continue;

            float interactionStrength = static_cast<float>(
                amouranth->computeInteraction(pair.vertexIndex, pair.distance) *
                std::exp(-glm::abs(amouranth->getAlpha() * pair.distance)) *
                amouranth->computePermeation(pair.vertexIndex) *
                glm::max(0.0f, static_cast<float>(pair.strength))
            );
            interactionStrength = glm::max(interactionStrength, 0.01f);
            interactionStrength = glm::min(interactionStrength, 2.0f);

            float offset = static_cast<float>(pair.distance) * 0.6f * (1.0f + static_cast<float>(pair.strength) * 0.3f);
            float angle = wavePhase + pair.vertexIndex * 2.0f + pair.distance * 0.2f;
            glm::vec3 offsetPos = glm::vec3(
                offset * cosf(angle),
                offset * sinf(angle),
                offset * 0.3f * sinf(angle * 0.6f)
            );
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), offsetPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(0.4f * zoomFactor, 0.4f * zoomFactor, 0.4f * zoomFactor));

            glm::vec3 baseColor = glm::vec3(
                0.8f - 0.2f * sinf(angle),
                0.9f - 0.2f * cosf(angle * 1.3f),
                1.0f - 0.1f * sinf(angle * 0.8f)
            );

            PushConstants pushConstants = {
                interactionModel,
                view,
                proj,
                baseColor,
                interactionStrength * (0.7f + 0.3f * cosf(wavePhase + pair.distance)),
                3.0f,
                wavePhase,
                cycleProgress,
                static_cast<float>(pair.strength),
                static_cast<float>(amouranth->computeDarkEnergy(pair.distance))
            };
            vkCmdPushConstants(commandBuffer, pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
        }
    }
}