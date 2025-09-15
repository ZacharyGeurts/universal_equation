#include "modes.hpp"
#include "main.hpp"
#include "types.hpp"

static constexpr int kMaxRenderedDimensions = 9;

void renderMode3(DimensionalNavigator* navigator, uint32_t imageIndex, VkBuffer vertexBuffer_,
                 std::vector<VkCommandBuffer>& commandBuffers_, VkBuffer indexBuffer_,
                 float zoomLevel_, int width_, int height_, float wavePhase_,
                 std::vector<DimensionData>& cache_) {
    // Bind vertex and index buffers for spheres
    VkBuffer vertexBuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffers_[imageIndex], indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

    // Camera setup: Stationary unless user moves it
    float zoomFactor = glm::max(zoomLevel_, 0.01f);
    float aspect = static_cast<float>(width_) / glm::max(1.0f, static_cast<float>(height_));
    glm::vec3 camPos = navigator->isUserCamActive_ ? navigator->userCamPos_ : glm::vec3(0.0f, 0.0f, 16.0f * zoomFactor);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    float cycleProgress = std::fmod(wavePhase_ / (3.0f * kMaxRenderedDimensions), 1.0f);

    if (cache_.size() < static_cast<size_t>(kMaxRenderedDimensions)) {
        std::cerr << "Warning: Cache size " << cache_.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    // Render the main sphere for dimension 3
    for (size_t i = 0; i < static_cast<size_t>(kMaxRenderedDimensions); ++i) {
        if (i != 2) continue; // Only render dimension 3
        if (cache_[i].dimension != 3) {
            std::cerr << "Warning: Invalid cache for dimension 3\n";
            continue;
        }

        // Scaling based on dimension properties
        float observableScale = 1.0f + static_cast<float>(cache_[i].observable) * 0.3f;
        float darkMatterScale = 1.0f + static_cast<float>(cache_[i].darkMatter) * 0.5f;
        float darkEnergyScale = 1.0f + static_cast<float>(cache_[i].darkEnergy) * 0.4f;
        float radius = 0.7f * observableScale * darkMatterScale * darkEnergyScale * (1.0f + 0.2f * sinf(wavePhase_));
        radius *= zoomFactor;
        radius = glm::clamp(radius, 0.1f * zoomFactor, 10.0f * zoomFactor);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(radius));
        model = glm::rotate(model, wavePhase_ * 0.23f, glm::normalize(glm::vec3(0.7f, 1.0f, 1.3f)));

        float dimValue = 3.0f;
        float value = glm::clamp(static_cast<float>(cache_[i].observable) * (0.8f + 0.2f * cosf(wavePhase_)), 0.01f, 1.0f);

        // Color with shimmer effect
        glm::vec3 baseColor = glm::vec3(
            1.0f - 0.19f * sinf(wavePhase_ * 0.85f + 2.0f),
            1.0f - 0.14f * cosf(wavePhase_ * 0.63f + 1.1f),
            1.0f - 0.21f * sinf(wavePhase_ * 0.7f + 2.7f)
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

        std::cerr << "Mode3[D=3]: radius=" << radius << ", value=" << value
                  << ", pos=(0, 0, 0), color=(" << baseColor.x << ", " << baseColor.y << ", " << baseColor.z << ")\n";
    }

    // Render interactions for dimension 3 as orbiting spheres
    navigator->ue_.setCurrentDimension(3);
    auto pairs = navigator->ue_.getInteractions();
    if (pairs.empty()) {
        std::cerr << "Warning: No interactions for dimension 3\n";
        glm::mat4 interactionModel = glm::mat4(1.0f);
        interactionModel = glm::translate(interactionModel, glm::vec3(0.0f, 0.0f, 0.0f));
        interactionModel = glm::scale(interactionModel, glm::vec3(0.35f * zoomFactor));
        glm::vec3 baseColor = glm::vec3(1.0f, 0.97f, 0.93f);
        PushConstants pushConstants = {
            interactionModel,
            view,
            proj,
            baseColor,
            0.5f,
            3.0f,
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
            if (pair.dimension != 1 && pair.dimension != 2 && pair.dimension != 3 && pair.dimension != 4) continue;

            float interactionStrength = static_cast<float>(
                navigator->computeInteraction(pair.dimension, pair.distance) *
                std::exp(-glm::abs(navigator->ue_.getAlpha() * pair.distance)) *
                navigator->computePermeation(pair.dimension) *
                glm::max(0.0f, static_cast<float>(pair.darkMatterDensity))
            );
            interactionStrength = glm::clamp(interactionStrength, 0.01f, 2.0f);

            // 3D orbit positioning
            float orbitRadius = 1.5f + static_cast<float>(pair.distance) * 0.5f * (1.0f + static_cast<float>(pair.darkMatterDensity) * 0.2f);
            float angleA = wavePhase_ + pair.dimension * 2.0f + pair.distance * 0.13f;
            float angleB = wavePhase_ * 0.7f + pair.dimension * 0.9f + pair.distance * 0.17f;
            glm::vec3 orbitPos = glm::vec3(
                cosf(angleA) * orbitRadius * zoomFactor,
                sinf(angleA) * orbitRadius * zoomFactor,
                cosf(angleB) * orbitRadius * 0.7f * zoomFactor
            );
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), orbitPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(0.35f * zoomFactor));

            // Interaction color
            glm::vec3 baseColor = glm::vec3(
                1.0f - 0.18f * sinf(angleA),
                1.0f - 0.13f * cosf(angleB * 1.12f),
                1.0f - 0.11f * sinf(angleB * 0.7f)
            );

            PushConstants pushConstants = {
                interactionModel,
                view,
                proj,
                baseColor,
                interactionStrength * (0.7f + 0.3f * cosf(wavePhase_ + pair.distance)),
                3.0f,
                wavePhase_,
                cycleProgress,
                static_cast<float>(pair.darkMatterDensity),
                static_cast<float>(navigator->computeDarkEnergy(pair.distance))
            };
            vkCmdPushConstants(commandBuffers_[imageIndex], navigator->pipelineLayout_,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->sphereIndices_.size()), 1, 0, 0, 0);

            std::cerr << "Mode3 Interaction[D=" << pair.dimension << "]: strength=" << interactionStrength
                      << ", orbitRadius=" << orbitRadius << ", pos=("
                      << orbitPos.x << ", " << orbitPos.y << ", " << orbitPos.z << ")\n";
        }
    }
}