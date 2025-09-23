#include "core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <vulkan/vulkan.h>
#include <iostream>
#include <cmath>

// Define kMaxRenderedDimensions to avoid private access
static constexpr int kMaxRenderedDimensions = 9;

void renderMode5(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    // Suppress unused parameter warning
    (void)imageIndex;

    // Bind vertex and index buffers (using sphere geometry)
    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Check sphere indices
    if (amouranth->getSphereIndices().empty()) {
        std::cerr << "Warning: Sphere indices empty\n";
        return;
    }

    // Ensure zoom is positive
    float zoomFactor = glm::max(zoomLevel, 0.01f);
    float aspect = static_cast<float>(width) / glm::max(1.0f, static_cast<float>(height));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);

    // Camera setup
    glm::vec3 camPos(0.0f, 0.0f, 12.0f * zoomFactor); // Adjusted for mode 5
    if (amouranth->isUserCamActive()) {
        camPos = amouranth->getUserCamPos();
    }
    glm::vec3 camTarget(0.0f, 0.0f, 0.0f);
    glm::vec3 camUp(0.0f, 1.0f, 0.0f);
    glm::mat4 view = glm::lookAt(camPos, camTarget, camUp);

    // Divine cycle adjusted for mode 5
    float cycleProgress = std::fmod(wavePhase / (2.0f * kMaxRenderedDimensions), 1.0f);

    if (cache.size() < kMaxRenderedDimensions) {
        std::cerr << "Warning: Cache size " << cache.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    // Render only dimension 5
    size_t i = 4; // Index for dimension 5 (0-based index)
    if (cache[i].dimension != 5) {
        std::cerr << "Warning: Invalid cache for dimension 5\n";
        return;
    }

    // Fractal oscillation adjusted for mode 5
    float osc = 1.0f + 0.3f * sinf(wavePhase * (1.0f + static_cast<float>(cache[i].darkEnergy) * 0.7f)); // Reduced amplitude
    float value = static_cast<float>(cache[i].observable * osc);
    value = glm::clamp(value, 0.01f, 1.5f); // Tighter clamp for mode 5

    // Positioning adjusted for mode 5
    float angle = wavePhase + 5 * 2.0f * glm::pi<float>() / kMaxRenderedDimensions;
    float scaleFactor = 1.0f + static_cast<float>(cache[i].observable) * 0.4f; // Reduced scale factor
    float radius = 4.0f * scaleFactor; // Smaller radius for mode 5
    glm::vec3 pos = glm::vec3(
        radius * cosf(angle + cycleProgress),
        radius * sinf(angle + cycleProgress),
        radius * sinf(wavePhase + i * 0.5f) * 0.4f // Reduced z oscillation
    );
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos);
    model = glm::scale(model, glm::vec3(0.4f * zoomFactor * osc * scaleFactor, 0.4f * zoomFactor * osc * scaleFactor, 0.4f * zoomFactor * osc * scaleFactor)); // Smaller scale
    model = glm::rotate(model, wavePhase * 0.6f, glm::vec3(sinf(i * 0.4f), cosf(i * 0.4f), 0.4f)); // Slower rotation

    // Color with adjusted fractal effect for mode 5
    glm::vec3 baseColor = glm::vec3(
        0.4f + 0.6f * cosf(wavePhase + i * 1.0f + cycleProgress), // Adjusted color frequencies
        0.3f + 0.5f * sinf(wavePhase + i * 0.8f),
        0.6f - 0.4f * cosf(wavePhase * 0.6f + i)
    );

    PushConstants pushConstants = {
        model,
        view,
        proj,
        baseColor,
        value,
        5.0f, // Dimension 5
        wavePhase,
        cycleProgress,
        static_cast<float>(cache[i].darkMatter),
        static_cast<float>(cache[i].darkEnergy)
    };
    vkCmdPushConstants(commandBuffer, pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);

    // Interactions for dimension 5
    amouranth->setCurrentDimension(5);
    auto pairs = amouranth->getInteractions();
    if (pairs.empty()) {
        std::cerr << "Warning: No interactions for dimension 5\n";
        glm::mat4 interactionModel = glm::mat4(1.0f);
        interactionModel = glm::translate(interactionModel, glm::vec3(0.0f, 0.0f, 0.0f));
        interactionModel = glm::scale(interactionModel, glm::vec3(0.4f * zoomFactor, 0.4f * zoomFactor, 0.4f * zoomFactor)); // Smaller scale
        glm::vec3 baseColor = glm::vec3(0.8f, 0.85f, 0.9f); // Slightly different default color
        PushConstants pushConstants = {
            interactionModel,
            view,
            proj,
            baseColor,
            0.4f, // Reduced value for default interaction
            5.0f, // Dimension 5
            wavePhase,
            cycleProgress,
            0.4f, // Adjusted default dark matter
            0.4f  // Adjusted default dark energy
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
    } else {
        for (const auto& pair : pairs) {
            if (amouranth->getMode() != 5) continue;

            float interactionStrength = static_cast<float>(
                amouranth->computeInteraction(pair.vertexIndex, pair.distance) *
                std::exp(-glm::abs(amouranth->getAlpha() * pair.distance)) *
                amouranth->computePermeation(pair.vertexIndex) *
                glm::max(0.0f, static_cast<float>(pair.strength))
            );
            interactionStrength = glm::max(interactionStrength, 0.01f);
            interactionStrength = glm::min(interactionStrength, 1.5f); // Tighter clamp for mode 5

            float offset = static_cast<float>(pair.distance) * 0.8f * (1.0f + static_cast<float>(pair.strength) * 0.5f); // Reduced offset
            float angle = wavePhase + pair.vertexIndex * 1.8f + pair.distance * 0.4f; // Adjusted angle factors
            glm::vec3 offsetPos = glm::vec3(
                offset * cosf(angle + cycleProgress),
                offset * sinf(angle + cycleProgress),
                offset * 0.4f * sinf(angle * 0.7f) // Reduced z oscillation
            );
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), offsetPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(0.25f * zoomFactor, 0.25f * zoomFactor, 0.25f * zoomFactor)); // Smaller interaction scale

            glm::vec3 baseColor = glm::vec3(
                0.6f - 0.2f * sinf(angle), // Adjusted color frequencies
                0.5f - 0.15f * cosf(angle * 1.4f),
                0.8f - 0.1f * sinf(angle * 1.0f)
            );

            PushConstants pushConstants = {
                interactionModel,
                view,
                proj,
                baseColor,
                interactionStrength * (0.6f + 0.2f * cosf(wavePhase + pair.distance)), // Reduced modulation
                5.0f, // Dimension 5
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