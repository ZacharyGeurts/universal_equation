#include "modes.hpp"
#include "main.hpp"
#include "types.hpp"

static constexpr int kMaxRenderedDimensions = 9;

void renderMode2(DimensionalNavigator* navigator, uint32_t imageIndex, VkBuffer vertexBuffer_,
                 std::vector<VkCommandBuffer>& commandBuffers_, VkBuffer indexBuffer_,
                 float zoomLevel_, int width_, int height_, float wavePhase_,
                 std::vector<DimensionData>& cache_) {
    // Bind vertex and index buffers
    VkBuffer vertexBuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffers_[imageIndex], indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

    // Ensure zoom is always positive and not too small
    float zoomFactor = glm::max(zoomLevel_, 0.01f);
    float aspect = static_cast<float>(width_) / glm::max(1.0f, static_cast<float>(height_));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);

    // Camera setup
    glm::vec3 camPos(0.0f, 0.0f, 10.0f * zoomFactor);
    glm::vec3 camTarget(0.0f, 0.0f, 0.0f);
    glm::vec3 camUp(0.0f, 1.0f, 0.0f);
    glm::mat4 view = glm::lookAt(camPos, camTarget, camUp);

    constexpr float baseRadius = 0.5f;
    float cycleProgress = std::fmod(wavePhase_ / (2.0f * kMaxRenderedDimensions), 1.0f);

    if (cache_.size() < static_cast<size_t>(kMaxRenderedDimensions)) {
        std::cerr << "Warning: Cache size " << cache_.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    for (size_t i = 0; i < static_cast<size_t>(kMaxRenderedDimensions); ++i) {
        if (cache_[i].dimension != static_cast<int>(i + 1)) {
            std::cerr << "Warning: Invalid cache for dimension " << i + 1 << "\n";
            continue;
        }

        // Emphasis for the second dimension
        float emphasis = (i == 1) ? 2.0f : 0.5f;
        float osc = (1.0f + 0.1f * sinf(wavePhase_ + i) * (1.0f + static_cast<float>(cache_[i].darkMatter) * 0.5f));
        float radius = baseRadius * emphasis * (1.0f + static_cast<float>(cache_[i].observable) * 0.2f) * osc;
        radius *= zoomFactor;
        radius = glm::clamp(radius, 0.1f * zoomFactor, 2.0f * zoomFactor);

        glm::mat4 model = glm::mat4(1.0f);
        float angle = wavePhase_ + (i + 1) * 2.0f * glm::pi<float>() / kMaxRenderedDimensions;
        float spacing = 1.5f * (1.0f + static_cast<float>(cache_[i].darkEnergy) * 0.5f);
        float x = cosf(angle) * spacing;
        float y = sinf(angle) * spacing;
        float z = -static_cast<float>(i) * 0.5f;

        if (i == 1) {
            model = glm::translate(model, glm::vec3(x * 1.5f * zoomFactor, y * 1.5f * zoomFactor, z));
            model = glm::scale(model, glm::vec3(radius));
        } else if (i == 0) {
            model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
            model = glm::scale(model, glm::vec3(radius));
        } else {
            model = glm::translate(model, glm::vec3(x * zoomFactor, y * zoomFactor, z));
            model = glm::scale(model, glm::vec3(radius));
        }

        float dimValue = static_cast<float>(i + 1);
        float value = static_cast<float>(cache_[i].observable);
        value = glm::clamp(value, 0.01f, 1.0f);

        // Color with shimmering effect
        glm::vec3 baseColor = glm::vec3(
            1.0f,
            1.0f - 0.18f * sinf(wavePhase_ * 0.75f + i),
            1.0f - 0.18f * cosf(wavePhase_ * 0.5f + i)
        );

        PushConstants pushConstants = {
            model,
            view,
            proj,
            baseColor,
            value,
            dimValue,
            wavePhase_,
            cycleProgress,
            static_cast<float>(cache_[i].darkMatter),
            static_cast<float>(cache_[i].darkEnergy)
        };
        vkCmdPushConstants(commandBuffers_[imageIndex], navigator->pipelineLayout_,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->sphereIndices_.size()), 1, 0, 0, 0);

        std::cerr << "Mode2[D=" << i + 1 << "]: radius=" << radius << ", value=" << value
                  << ", pos=(" << x << ", " << y << ", " << z << ")\n";
    }

    // Interactions for dimension 2
    navigator->ue_.setCurrentDimension(2);
    auto pairs = navigator->ue_.getInteractions();
    if (pairs.empty()) {
        std::cerr << "Warning: No interactions for dimension 2\n";
        glm::mat4 interactionModel = glm::mat4(1.0f);
        interactionModel = glm::translate(interactionModel, glm::vec3(0.0f, 0.0f, 0.0f));
        interactionModel = glm::scale(interactionModel, glm::vec3(0.5f * zoomFactor));
        glm::vec3 baseColor = glm::vec3(1.0f, 0.97f, 0.95f);
        PushConstants pushConstants = {
            interactionModel,
            view,
            proj,
            baseColor,
            0.5f,
            2.0f,
            wavePhase_,
            cycleProgress,
            0.5f,
            0.5f
        };
        vkCmdPushConstants(commandBuffers_[imageIndex], navigator->pipelineLayout_,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->sphereIndices_.size()), 1, 0, 0, 0);
    } else {
        for (const auto& pair : pairs) {
            if (pair.dimension != 2) continue;

            float interactionStrength = static_cast<float>(
                navigator->computeInteraction(pair.dimension, pair.distance) *
                std::exp(-glm::abs(navigator->ue_.getAlpha() * pair.distance)) *
                navigator->computePermeation(pair.dimension) *
                glm::max(0.0, pair.darkMatterDensity)
            );
            interactionStrength = glm::max(interactionStrength, 0.01f);
            interactionStrength = glm::min(interactionStrength, 2.0f);

            float offset = static_cast<float>(pair.distance) * 0.5f * (1.0f + static_cast<float>(pair.darkMatterDensity) * 0.2f);
            float angle = wavePhase_ + pair.dimension * 2.0f + pair.distance * 0.1f;
            glm::vec3 offsetPos = glm::vec3(
                offset * sinf(angle) * zoomFactor,
                offset * cosf(angle) * zoomFactor,
                0.0f
            );
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), offsetPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(0.5f * zoomFactor));

            glm::vec3 baseColor = glm::vec3(
                1.0f - 0.22f * sinf(angle),
                1.0f - 0.18f * cosf(angle * 1.2f),
                1.0f - 0.09f * sinf(angle * 0.7f)
            );

            PushConstants pushConstants = {
                interactionModel,
                view,
                proj,
                baseColor,
                interactionStrength * (0.7f + 0.3f * cosf(wavePhase_ + pair.distance)),
                2.0f,
                wavePhase_,
                cycleProgress,
                static_cast<float>(pair.darkMatterDensity),
                static_cast<float>(navigator->computeDarkEnergy(pair.distance))
            };
            vkCmdPushConstants(commandBuffers_[imageIndex], navigator->pipelineLayout_,
                              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                              0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->sphereIndices_.size()), 1, 0, 0, 0);
        }
    }
}