#include "modes.hpp"
#include "main.hpp"

void renderMode1(DimensionalNavigator* navigator, uint32_t imageIndex) {
    // Bind vertex and index buffers
    VkBuffer vertexBuffers[] = {navigator->quadVertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(navigator->commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(navigator->commandBuffers_[imageIndex], navigator->quadIndexBuffer_, 0, VK_INDEX_TYPE_UINT32);

    // Ensure zoom is always positive and not too small
    float zoomFactor = glm::max(navigator->zoomLevel_, 0.01f);
    float aspect = static_cast<float>(navigator->width_) / glm::max(1.0f, static_cast<float>(navigator->height_));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);

    // Camera setup
    glm::vec3 camPos(0.0f, 0.0f, 10.0f * zoomFactor);
    glm::vec3 camTarget(0.0f, 0.0f, 0.0f);
    glm::vec3 camUp(0.0f, 1.0f, 0.0f);
    glm::mat4 view = glm::lookAt(camPos, camTarget, camUp);

    // Divine cycle: smoothly oscillates in [0, 1)
    float cycleProgress = std::fmod(navigator->wavePhase_ / (2.0f * navigator->kMaxRenderedDimensions), 1.0f);

    if (navigator->cache_.size() < navigator->kMaxRenderedDimensions) {
        std::cerr << "Warning: Cache size " << navigator->cache_.size() << " < " << navigator->kMaxRenderedDimensions << "\n";
        return;
    }

    for (size_t i = 0; i < navigator->kMaxRenderedDimensions; ++i) {
        if (i > 0) continue;
        if (navigator->cache_[i].dimension != 1) {
            std::cerr << "Warning: Invalid cache for dimension 1\n";
            continue;
        }

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(100.0f * zoomFactor, 100.0f * zoomFactor, 0.01f));
        model = glm::rotate(model, navigator->wavePhase_ * 0.1f, glm::vec3(0.0f, 1.0f, 0.0f));

        // Oscillation for observable
        float dimValue = 1.0f;
        float osc = 0.8f + 0.2f * cosf(navigator->wavePhase_ + i);
        float value = static_cast<float>(navigator->cache_[i].observable * osc);
        value = glm::clamp(value, 0.01f, 1.0f);

        // Color with shimmering aura
        glm::vec3 baseColor = glm::vec3(
            1.0f,
            1.0f - 0.25f * sinf(navigator->wavePhase_ * 0.75f + i),
            1.0f - 0.25f * cosf(navigator->wavePhase_ * 0.5f + i)
        );

        PushConstants pushConstants = {
            model,
            view,
            proj,
            baseColor,
            value,
            dimValue,
            navigator->wavePhase_,
            cycleProgress,
            static_cast<float>(navigator->cache_[i].darkMatter),
            static_cast<float>(navigator->cache_[i].darkEnergy)
        };
        vkCmdPushConstants(navigator->commandBuffers_[imageIndex], navigator->pipelineLayout_,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(navigator->commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->quadIndices_.size()), 1, 0, 0, 0);
    }

    // Interactions for dimension 1
    navigator->ue_.setCurrentDimension(1);
    auto pairs = navigator->ue_.getInteractions();
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
            navigator->wavePhase_,
            cycleProgress,
            0.5f,
            0.5f
        };
        vkCmdPushConstants(navigator->commandBuffers_[imageIndex], navigator->pipelineLayout_,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(navigator->commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->quadIndices_.size()), 1, 0, 0, 0);
    } else {
        for (const auto& pair : pairs) {
            if (navigator->ue_.getCurrentDimension() > 1) continue;

            // Interaction strength
            float interactionStrength = static_cast<float>(
                navigator->computeInteraction(pair.vertexIndex, pair.distance) *
                std::exp(-glm::abs(navigator->ue_.getAlpha() * pair.distance)) *
                navigator->computePermeation(pair.vertexIndex) *
                glm::max(0.0f, static_cast<float>(pair.strength))
            );
            interactionStrength = glm::max(interactionStrength, 0.01f);
            interactionStrength = glm::min(interactionStrength, 2.0f);

            // Offset visual
            float offset = static_cast<float>(pair.distance) * 0.5f * (1.0f + static_cast<float>(pair.strength) * 0.2f);
            float angle = navigator->wavePhase_ + pair.vertexIndex * 2.0f + pair.distance * 0.1f;
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
                interactionStrength * (0.7f + 0.3f * cosf(navigator->wavePhase_ + pair.distance)),
                1.0f,
                navigator->wavePhase_,
                cycleProgress,
                static_cast<float>(pair.strength),
                static_cast<float>(navigator->computeDarkEnergy(pair.distance))
            };
            vkCmdPushConstants(navigator->commandBuffers_[imageIndex], navigator->pipelineLayout_,
                              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                              0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(navigator->commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->quadIndices_.size()), 1, 0, 0, 0);
        }
    }
}