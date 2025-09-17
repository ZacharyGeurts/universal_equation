#include "modes.hpp"
#include "main.hpp"
#include "types.hpp"

static constexpr int kMaxRenderedDimensions = 9;

void renderMode6(DimensionalNavigator* navigator, uint32_t imageIndex, VkBuffer vertexBuffer_,
                 std::vector<VkCommandBuffer>& commandBuffers_, VkBuffer indexBuffer_,
                 float zoomLevel_, int width_, int height_, float wavePhase_,
                 std::vector<DimensionData>& cache_) {
    VkBuffer vertexBuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffers_[imageIndex], indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

    float zoomFactor = glm::max(zoomLevel_, 0.01f);
    float aspect = static_cast<float>(width_) / glm::max(1.0f, static_cast<float>(height_));
    glm::vec3 camPos = navigator->isUserCamActive_ ? navigator->userCamPos_ : glm::vec3(0.0f, 0.0f, 26.0f * zoomFactor);
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    float cycleProgress = std::fmod(wavePhase_ / (6.0f * kMaxRenderedDimensions), 1.0f);
    float divineGlow = 0.92f + 0.08f * sinf(wavePhase_ * 0.35f);

    if (cache_.size() < static_cast<size_t>(kMaxRenderedDimensions)) {
        std::cerr << "Warning: Cache size " << cache_.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    for (size_t i = 0; i < static_cast<size_t>(kMaxRenderedDimensions); ++i) {
        if (cache_[i].dimension != static_cast<int>(i + 1)) {
            std::cerr << "Warning: Invalid cache for dimension " << i + 1 << "\n";
            continue;
        }

        float observableScale = 1.0f + static_cast<float>(cache_[i].observable) * 0.5f;
        float darkMatterScale = 1.0f + static_cast<float>(cache_[i].darkMatter) * 0.75f;
        float darkEnergyScale = 1.0f + static_cast<float>(cache_[i].darkEnergy) * 0.6f;
        float radius = 1.1f * observableScale * darkMatterScale * darkEnergyScale * (1.0f + 0.4f * sinf(wavePhase_ + i));
        radius *= zoomFactor;
        radius = glm::clamp(radius, 0.3f * zoomFactor, 12.0f * zoomFactor);

        float angle = wavePhase_ + (i + 1) * 2.0f * glm::pi<float>() / kMaxRenderedDimensions;
        float spacing = 3.1f * (1.0f + static_cast<float>(cache_[i].darkEnergy) * 0.9f);
        glm::vec3 pos;
        switch (i) {
            case 0: pos = glm::vec3(0.0f, 0.0f, 0.0f); break;
            case 1: pos = glm::vec3(spacing * cosf(angle), spacing * sinf(angle), 0.0f); break;
            case 2: pos = glm::vec3(spacing * cosf(angle + 2.0f * glm::pi<float>() / 3.0f), 
                                    spacing * sinf(angle + 2.0f * glm::pi<float>() / 3.0f), spacing); break;
            case 3: pos = glm::vec3(spacing * cosf(angle + 4.0f * glm::pi<float>() / 3.0f), 
                                    spacing * sinf(angle + 4.0f * glm::pi<float>() / 3.0f), -spacing); break;
            case 4: pos = glm::vec3(spacing * cosf(angle + glm::pi<float>()), 0.0f, spacing * sinf(angle)); break;
            case 5: pos = glm::vec3(0.0f, spacing * sinf(angle), spacing * cosf(angle)); break;
            case 6: pos = glm::vec3(spacing * cosf(angle + glm::pi<float>() / 2.0f), spacing * sinf(angle), 0.0f); break;
            case 7: pos = glm::vec3(spacing * cosf(angle), 0.0f, spacing * sinf(angle + glm::pi<float>())); break;
            case 8: pos = glm::vec3(0.0f, 0.0f, 2.4f * spacing * sinf(wavePhase_ + i)); break;
        }
        pos *= zoomFactor;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, pos);
        model = glm::scale(model, glm::vec3(radius));
        model = glm::rotate(model, wavePhase_ * 0.3f + i * 0.15f, glm::normalize(glm::vec3(0.9f, 0.1f, 0.5f + 0.1f * i)));

        float dimValue = static_cast<float>(i + 1);
        float value = glm::clamp(static_cast<float>(cache_[i].observable) * divineGlow, 0.01f, 1.0f);

        glm::vec3 baseColor = glm::vec3(
            0.75f + 0.45f * sinf(wavePhase_ * 0.89f + i),
            0.6f + 0.4f * cosf(wavePhase_ * 0.71f + i),
            0.85f + 0.45f * sinf(wavePhase_ * 1.25f + i)
        );
        baseColor = glm::clamp(baseColor, 0.0f, 1.0f);

        PushConstants pushConstants = {
            model,
            view,
            proj,
            baseColor,
            value,
            dimValue,
            wavePhase_,
            cycleProgress,
            static_cast<float>(cache_[i].darkMatter) * divineGlow,
            static_cast<float>(cache_[i].darkEnergy) * divineGlow
        };
        vkCmdPushConstants(commandBuffers_[imageIndex], navigator->pipelineLayout_,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->sphereIndices_.size()), 1, 0, 0, 0);

        std::cerr << "Mode6[D=" << i + 1 << "]: radius=" << radius << ", value=" << value
                  << ", pos=(" << pos.x << ", " << pos.y << ", " << pos.z << "), color=("
                  << baseColor.x << ", " << baseColor.y << ", " << baseColor.z << ")\n";
    }

    for (size_t i = 0; i < static_cast<size_t>(kMaxRenderedDimensions); ++i) {
        navigator->ue_.setCurrentDimension(i + 1);
        auto pairs = navigator->ue_.getInteractions();
        if (pairs.empty()) {
            std::cerr << "Warning: No interactions for dimension " << i + 1 << "\n";
            continue;
        }

        for (const auto& pair : pairs) {
            if (navigator->ue_.getCurrentDimension() != static_cast<int>(i + 1)) continue;

            float interactionStrength = static_cast<float>(
                navigator->computeInteraction(pair.vertexIndex, pair.distance) *
                std::exp(-glm::abs(navigator->ue_.getAlpha() * pair.distance)) *
                navigator->computePermeation(pair.vertexIndex) *
                glm::max(0.0f, static_cast<float>(pair.strength)));
            interactionStrength *= divineGlow;
            interactionStrength = glm::clamp(interactionStrength, 0.01f, 2.5f);

            float orbitRadius = 2.4f + static_cast<float>(pair.distance) * 0.55f * (1.0f + static_cast<float>(pair.strength) * 0.4f);
            float angleA = wavePhase_ + pair.vertexIndex * 2.4f + pair.distance * 0.17f;
            float angleB = wavePhase_ * 0.9f + pair.vertexIndex * 1.1f + pair.distance * 0.21f;
            glm::vec3 orbitPos = glm::vec3(
                cosf(angleA) * orbitRadius * zoomFactor,
                sinf(angleA) * orbitRadius * zoomFactor,
                sinf(angleB + pair.vertexIndex) * orbitRadius * 0.7f * zoomFactor
            );
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), orbitPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(0.44f * zoomFactor * (1.0f + divineGlow)));

            glm::vec3 interactionColor = glm::vec3(
                0.8f + 0.4f * sinf(wavePhase_ * 0.87f + pair.vertexIndex),
                0.65f + 0.35f * cosf(wavePhase_ * 0.69f + pair.vertexIndex),
                0.9f + 0.4f * sinf(wavePhase_ * 1.23f + pair.vertexIndex)
            );
            interactionColor = glm::clamp(interactionColor, 0.0f, 1.0f);

            PushConstants pushConstants = {
                interactionModel,
                view,
                proj,
                interactionColor,
                interactionStrength,
                static_cast<float>(i + 1),
                wavePhase_,
                cycleProgress,
                static_cast<float>(pair.strength) * divineGlow,
                static_cast<float>(navigator->computeDarkEnergy(pair.distance)) * divineGlow
            };
            vkCmdPushConstants(commandBuffers_[imageIndex], navigator->pipelineLayout_,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->sphereIndices_.size()), 1, 0, 0, 0);

            std::cerr << "Mode6 Interaction[D=" << i + 1 << "]: strength=" << interactionStrength
                      << ", orbitRadius=" << orbitRadius << ", color=("
                      << interactionColor.x << ", " << interactionColor.y << ", " << interactionColor.z << ")\n";
        }
    }
}