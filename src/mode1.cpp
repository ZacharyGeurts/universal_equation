#include "core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <vulkan/vulkan.h>
#include <iostream>
#include <cmath>

static constexpr int kMaxRenderedDimensions = 9;

void renderMode1(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    (void)imageIndex;

    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    if (amouranth->getSphereIndices().empty()) {
        std::cerr << "Warning: Sphere indices empty\n";
        return;
    }

    float zoomFactor = glm::max(zoomLevel, 0.01f);
    float aspect = static_cast<float>(width) / glm::max(1.0f, static_cast<float>(height));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);

    glm::vec3 camPos(0.0f, 0.0f, 8.0f * zoomFactor); // Close camera for dimension 1
    if (amouranth->isUserCamActive()) {
        camPos = amouranth->getUserCamPos();
    }
    glm::vec3 camTarget(0.0f, 0.0f, 0.0f);
    glm::vec3 camUp(0.0f, 1.0f, 0.0f);
    glm::mat4 view = glm::lookAt(camPos, camTarget, camUp);

    float cycleProgress = std::fmod(wavePhase / (2.0f * kMaxRenderedDimensions), 1.0f);

    if (cache.size() < kMaxRenderedDimensions) {
        std::cerr << "Warning: Cache size " << cache.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    size_t i = 0; // Dimension 1
    if (cache[i].dimension != 1) {
        std::cerr << "Warning: Invalid cache for dimension 1\n";
        return;
    }

    float osc = 1.0f + 0.1f * sinf(wavePhase * (1.0f + static_cast<float>(cache[i].darkEnergy) * 0.5f)); // Minimal oscillation
    float value = static_cast<float>(cache[i].observable * osc);
    value = glm::clamp(value, 0.01f, 1.0f); // Tight clamp for dimension 1

    float angle = wavePhase + 1 * 2.0f * glm::pi<float>() / kMaxRenderedDimensions;
    float scaleFactor = 1.0f + static_cast<float>(cache[i].observable) * 0.2f; // Minimal scale factor
    float radius = 2.0f * scaleFactor; // Small radius
    glm::vec3 pos = glm::vec3(
        radius * cosf(angle + cycleProgress),
        radius * sinf(angle + cycleProgress),
        0.0f // Flat z for dimension 1
    );
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos);
    model = glm::scale(model, glm::vec3(0.2f * zoomFactor * osc * scaleFactor, 0.2f * zoomFactor * osc * scaleFactor, 0.2f * zoomFactor * osc * scaleFactor)); // Small scale
    model = glm::rotate(model, wavePhase * 0.4f, glm::vec3(sinf(i * 0.3f), cosf(i * 0.3f), 0.3f)); // Slow rotation

    glm::vec3 baseColor = glm::vec3(
        0.5f + 0.5f * cosf(wavePhase + i * 0.8f + cycleProgress), // Simple color variation
        0.2f + 0.3f * sinf(wavePhase + i * 0.6f),
        0.7f - 0.3f * cosf(wavePhase * 0.5f + i)
    );

    PushConstants pushConstants = {
        model,
        view,
        proj,
        baseColor,
        value,
        1.0f, // Dimension 1
        wavePhase,
        cycleProgress,
        static_cast<float>(cache[i].darkMatter),
        static_cast<float>(cache[i].darkEnergy)
    };
    vkCmdPushConstants(commandBuffer, pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);

    amouranth->setCurrentDimension(1);
    auto pairs = amouranth->getInteractions();
    if (pairs.empty()) {
        std::cerr << "Warning: No interactions for dimension 1\n";
        glm::mat4 interactionModel = glm::mat4(1.0f);
        interactionModel = glm::translate(interactionModel, glm::vec3(0.0f, 0.0f, 0.0f));
        interactionModel = glm::scale(interactionModel, glm::vec3(0.2f * zoomFactor, 0.2f * zoomFactor, 0.2f * zoomFactor));
        glm::vec3 baseColor = glm::vec3(0.7f, 0.8f, 0.85f);
        PushConstants pushConstants = {
            interactionModel,
            view,
            proj,
            baseColor,
            0.3f,
            1.0f,
            wavePhase,
            cycleProgress,
            0.3f,
            0.3f
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
    } else {
        for (const auto& pair : pairs) {
            if (amouranth->getMode() != 1) continue;

            float interactionStrength = static_cast<float>(
                amouranth->computeInteraction(pair.vertexIndex, pair.distance) *
                std::exp(-glm::abs(amouranth->getAlpha() * pair.distance)) *
                amouranth->computePermeation(pair.vertexIndex) *
                glm::max(0.0f, static_cast<float>(pair.strength))
            );
            interactionStrength = glm::max(interactionStrength, 0.01f);
            interactionStrength = glm::min(interactionStrength, 1.0f);

            float offset = static_cast<float>(pair.distance) * 0.6f * (1.0f + static_cast<float>(pair.strength) * 0.3f); // Minimal offset
            float angle = wavePhase + pair.vertexIndex * 1.5f + pair.distance * 0.3f;
            glm::vec3 offsetPos = glm::vec3(
                offset * cosf(angle + cycleProgress),
                offset * sinf(angle + cycleProgress),
                0.0f // Flat z
            );
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), offsetPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(0.15f * zoomFactor, 0.15f * zoomFactor, 0.15f * zoomFactor));

            glm::vec3 baseColor = glm::vec3(
                0.7f - 0.1f * sinf(angle),
                0.4f - 0.1f * cosf(angle * 1.2f),
                0.9f - 0.1f * sinf(angle * 0.8f)
            );

            PushConstants pushConstants = {
                interactionModel,
                view,
                proj,
                baseColor,
                interactionStrength * (0.5f + 0.1f * cosf(wavePhase + pair.distance)),
                1.0f,
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