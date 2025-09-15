#include "modes.hpp"
#include "main.hpp"
#include "types.hpp"

static constexpr int kMaxRenderedDimensions = 9;

void renderMode4(DimensionalNavigator* navigator, uint32_t imageIndex, VkBuffer vertexBuffer_,
                 std::vector<VkCommandBuffer>& commandBuffers_, VkBuffer indexBuffer_,
                 float zoomLevel_, int width_, int height_, float wavePhase_,
                 std::vector<DimensionData>& cache_) {
    VkBuffer vertexBuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffers_[imageIndex], indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

    // Camera setup: Stationary unless user moves it
    float zoomFactor = glm::max(zoomLevel_, 0.01f);
    float aspect = static_cast<float>(width_) / glm::max(1.0f, static_cast<float>(height_));
    glm::vec3 camPos = navigator->isUserCamActive_ ? navigator->userCamPos_ : glm::vec3(0.0f, 0.0f, 13.0f * zoomFactor);
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    float cycleProgress = std::fmod(wavePhase_ / (4.0f * kMaxRenderedDimensions), 1.0f);

    if (cache_.size() < static_cast<size_t>(kMaxRenderedDimensions)) {
        std::cerr << "Warning: Cache size " << cache_.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    for (size_t i = 0; i < static_cast<size_t>(kMaxRenderedDimensions); ++i) {
        if (i != 3) continue;
        if (cache_[i].dimension != 4) {
            std::cerr << "Warning: Invalid cache for dimension 4\n";
            continue;
        }

        // Hyperspherical scaling, dynamic morphing
        float alpha = 2.0f;
        float omega = 0.33f;
        float observableRadius = 1.0f + static_cast<float>(cache_[i].observable) * 0.25f;
        float potentialRadius = 1.0f + static_cast<float>(cache_[i].potential) * 0.25f;
        float timeModulation = sinf(wavePhase_ * 1.5f + i) * (1.0f + static_cast<float>(cache_[i].darkMatter) * 0.6f);
        float geometryScale = 0.5f * (observableRadius + potentialRadius);
        float scaledGeometry = geometryScale * (alpha + omega * timeModulation);
        float morph = 1.0f + 0.35f * sinf(wavePhase_ * 0.73f + 2.0f);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(scaledGeometry * zoomFactor * 0.82f * morph,
                                            scaledGeometry * zoomFactor * 1.18f * morph,
                                            scaledGeometry * zoomFactor * morph));
        model = glm::rotate(model, wavePhase_ * 0.19f, glm::normalize(glm::vec3(0.7f, 1.0f, 0.7f)));

        float fluctuationRatio = cache_[i].potential > 0 ? static_cast<float>(cache_[i].observable) / static_cast<float>(cache_[i].potential) : 1.0f;
        float dimValue = 4.0f;

        // Transcendental 4D color: shimmering hyperspectral
        glm::vec3 baseColor = glm::vec3(
            0.55f + 0.27f * sinf(wavePhase_ * 0.97f + 0.8f),
            0.41f + 0.23f * cosf(wavePhase_ * 0.79f + 2.2f),
            0.68f + 0.29f * sinf(wavePhase_ * 1.41f + 1.7f)
        );

        PushConstants pushConstants = {
            model,
            view,
            proj,
            baseColor,
            fluctuationRatio * (0.8f + 0.4f * cosf(wavePhase_ * 1.2f)),
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

        std::cerr << "Mode4[D=4]: scaledGeometry=" << scaledGeometry << ", fluctuationRatio=" << fluctuationRatio
                  << ", pos=(0, 0, 0), color=(" << baseColor.x << ", " << baseColor.y << ", " << baseColor.z << ")\n";
    }

    // Render transcendental 4D interactions as tesseract orbits in 3D projection
    navigator->ue_.setCurrentDimension(4);
    auto pairs = navigator->ue_.getInteractions();
    if (pairs.empty()) {
        std::cerr << "Warning: No interactions for dimension 4\n";
        glm::mat4 interactionModel = glm::mat4(1.0f);
        interactionModel = glm::translate(interactionModel, glm::vec3(0.0f, 0.0f, 0.0f));
        interactionModel = glm::scale(interactionModel, glm::vec3(0.33f * zoomFactor));
        glm::vec3 baseColor = glm::vec3(
            0.55f + 0.27f * sinf(wavePhase_ * 0.97f),
            0.41f + 0.23f * cosf(wavePhase_ * 0.79f),
            0.68f + 0.29f * sinf(wavePhase_ * 1.41f)
        );
        PushConstants pushConstants = {
            interactionModel,
            view,
            proj,
            baseColor,
            0.5f,
            4.0f,
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
            if (pair.dimension < 1 || pair.dimension > 5) continue;

            // Hyperspatial interaction strength
            float interactionStrength = static_cast<float>(
                navigator->computeInteraction(pair.dimension, pair.distance) *
                std::exp(-glm::abs(navigator->ue_.getAlpha() * pair.distance)) *
                navigator->computePermeation(pair.dimension) *
                glm::max(0.0f, static_cast<float>(pair.darkMatterDensity))
            );
            interactionStrength = glm::clamp(interactionStrength, 0.01f, 2.5f);

            // Tesseract-inspired 4D orbit projection
            float orbitRadius = 2.0f + static_cast<float>(pair.distance) * 0.57f * (1.0f + static_cast<float>(pair.darkMatterDensity) * 0.33f);
            float angleA = wavePhase_ * 1.17f + pair.dimension * 1.47f;
            float angleB = wavePhase_ * 0.87f + pair.dimension * 1.23f;
            glm::vec3 orbitPos = glm::vec3(
                cosf(angleA) * orbitRadius * zoomFactor,
                sinf(angleA) * orbitRadius * zoomFactor,
                cosf(angleB) * orbitRadius * 0.87f * zoomFactor
            );
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), orbitPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(0.28f * zoomFactor * (1.0f + 0.22f * sinf(wavePhase_ + pair.dimension))));

            // Hyperspectral color for 4D interactions
            glm::vec3 baseColor = glm::vec3(
                0.55f + 0.27f * sinf(wavePhase_ * 0.97f + pair.dimension),
                0.41f + 0.23f * cosf(wavePhase_ * 0.79f + pair.dimension),
                0.68f + 0.29f * sinf(wavePhase_ * 1.41f + pair.dimension)
            );
            PushConstants pushConstants = {
                interactionModel,
                view,
                proj,
                baseColor,
                interactionStrength * (0.8f + 0.4f * cosf(wavePhase_ * 1.2f + pair.dimension)),
                4.0f,
                wavePhase_,
                cycleProgress,
                static_cast<float>(pair.darkMatterDensity),
                static_cast<float>(navigator->computeDarkEnergy(pair.distance))
            };
            vkCmdPushConstants(commandBuffers_[imageIndex], navigator->pipelineLayout_,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->sphereIndices_.size()), 1, 0, 0, 0);

            std::cerr << "Mode4 Interaction[D=" << pair.dimension << "]: strength=" << interactionStrength
                      << ", orbitRadius=" << orbitRadius << ", pos=("
                      << orbitPos.x << ", " << orbitPos.y << ", " << orbitPos.z << ")\n";
        }
    }
}