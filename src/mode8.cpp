#include "core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <vulkan/vulkan.h>
#include <iostream>
#include <cmath>

static constexpr int kMaxRenderedDimensions = 9;

void renderMode8(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
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

    glm::vec3 camPos(0.0f, 0.0f, 14.0f * zoomFactor); // Further for higher dimension
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

    size_t i = 7; // Dimension 8
    if (cache[i].dimension != 8) {
        std::cerr << "Warning: Invalid cache for dimension 8\n";
        return;
    }

    float osc = 1.0f + 0.35f * sinf(wavePhase * (1.0f + static_cast<float>(cache[i].darkEnergy) * 0.85f));
    float value = static_cast<float>(cache[i].observable * osc);
    value = glm::clamp(value, 0.01f, 1.8f);

    float angle = wavePhase + 8 * 2.0f * glm::pi<float>() / kMaxRenderedDimensions;
    float scaleFactor = 1.0f + static_cast<float>(cache[i].observable) * 0.45f;
    float radius = 4.8f * scaleFactor;
    glm::vec3 pos = glm::vec3(
        radius * cosf(angle + cycleProgress),
        radius * sinf(angle + cycleProgress),
        radius * sinf(wavePhase + i * 0.65f) * 0.45f
    );
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos);
    model = glm::scale(model, glm::vec3(0.45f * zoomFactor * osc * scaleFactor, 0.45f * zoomFactor * osc * scaleFactor, 0.45f * zoomFactor * osc * scaleFactor));
    model = glm::rotate(model, wavePhase * 0.65f, glm::vec3(sinf(i * 0.45f), cosf(i * 0.45f), 0.45f));

    glm::vec3 baseColor = glm::vec3(
        0.35f + 0.65f * cosf(wavePhase + i * 1.05f + cycleProgress),
        0.35f + 0.55f * sinf(wavePhase + i * 0.85f),
        0.55f - 0.45f * cosf(wavePhase * 0.65f + i)
    );

    PushConstants pushConstants = {
        model,
        view,
        proj,
        baseColor,
        value,
        8.0f,
        wavePhase,
        cycleProgress,
        static_cast<float>(cache[i].darkMatter),
        static_cast<float>(cache[i].darkEnergy)
    };
    vkCmdPushConstants(commandBuffer, pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);

    amouranth->setCurrentDimension(8);
    auto pairs = amouranth->getInteractions();
    if (pairs.empty()) {
        std::cerr << "Warning: No interactions for dimension 8\n";
        glm::mat4 interactionModel = glm::mat4(1.0f);
        interactionModel = glm::translate(interactionModel, glm::vec3(0.0f, 0.0f, 0.0f));
        interactionModel = glm::scale(interactionModel, glm::vec3(0.45f * zoomFactor, 0.45f * zoomFactor, 0.45f * zoomFactor));
        glm::vec3 baseColor = glm::vec3(0.85f, 0.9f, 0.95f);
        PushConstants pushConstants = {
            interactionModel,
            view,
            proj,
            baseColor,
            0.45f,
            8.0f,
            wavePhase,
            cycleProgress,
            0.45f,
            0.45f
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
    } else {
        for (const auto& pair : pairs) {
            if (amouranth->getMode() != 8) continue;

            float interactionStrength = static_cast<float>(
                amouranth->computeInteraction(pair.vertexIndex, pair.distance) *
                std::exp(-glm::abs(amouranth->getAlpha() * pair.distance)) *
                amouranth->computePermeation(pair.vertexIndex) *
                glm::max(0.0f, static_cast<float>(pair.strength))
            );
            interactionStrength = glm::max(interactionStrength, 0.01f);
            interactionStrength = glm::min(interactionStrength, 1.8f);

            float offset = static_cast<float>(pair.distance) * 0.85f * (1.0f + static_cast<float>(pair.strength) * 0.55f);
            float angle = wavePhase + pair.vertexIndex * 1.9f + pair.distance * 0.45f;
            glm::vec3 offsetPos = glm::vec3(
                offset * cosf(angle + cycleProgress),
                offset * sinf(angle + cycleProgress),
                offset * 0.45f * sinf(angle * 0.75f)
            );
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), offsetPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(0.28f * zoomFactor, 0.28f * zoomFactor, 0.28f * zoomFactor));

            glm::vec3 baseColor = glm::vec3(
                0.55f - 0.25f * sinf(angle),
                0.55f - 0.2f * cosf(angle * 1.5f),
                0.75f - 0.15f * sinf(angle * 1.05f)
            );

            PushConstants pushConstants = {
                interactionModel,
                view,
                proj,
                baseColor,
                interactionStrength * (0.65f + 0.25f * cosf(wavePhase + pair.distance)),
                8.0f,
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