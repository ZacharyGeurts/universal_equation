#include "modes.hpp"
#include "main.hpp"
#include "types.hpp"

static constexpr int kMaxRenderedDimensions = 9;

void renderMode5(DimensionalNavigator* navigator, uint32_t imageIndex, VkBuffer vertexBuffer_,
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
    glm::vec3 camPos = navigator->isUserCamActive_ ? navigator->userCamPos_ : glm::vec3(0.0f, 0.0f, 18.0f * zoomFactor);
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    float cycleProgress = std::fmod(wavePhase_ / (5.0f * kMaxRenderedDimensions), 1.0f);
    float heavenGlow = 0.82f + 0.18f * sinf(wavePhase_ * 0.51f);
    glm::vec3 pinkBaseColor = glm::vec3(1.0f, 0.41f, 0.71f);

    if (cache_.size() < static_cast<size_t>(kMaxRenderedDimensions)) {
        std::cerr << "Warning: Cache size " << cache_.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    // Render all dimensions as interconnected spheres (pentachoron/tetrahedral projection)
    for (size_t i = 0; i < static_cast<size_t>(kMaxRenderedDimensions); ++i) {
        if (cache_[i].dimension != static_cast<int>(i + 1)) {
            std::cerr << "Warning: Invalid cache for dimension " << i + 1 << "\n";
            continue;
        }

        // UniversalEquation math for scaling
        float observableScale = 1.0f + static_cast<float>(cache_[i].observable) * 0.3f;
        float darkMatterScale = 1.0f + static_cast<float>(cache_[i].darkMatter) * 0.5f;
        float darkEnergyScale = 1.0f + static_cast<float>(cache_[i].darkEnergy) * 0.4f;
        float radius = 0.7f * observableScale * darkMatterScale * darkEnergyScale * (1.0f + 0.2f * sinf(wavePhase_ + i));
        radius *= zoomFactor;
        radius = glm::clamp(radius, 0.1f * zoomFactor, 8.0f * zoomFactor);

        // 5D pentachoron-inspired arrangement
        float angle = wavePhase_ + (i + 1) * 2.0f * glm::pi<float>() / kMaxRenderedDimensions;
        float spacing = 2.3f * (1.0f + static_cast<float>(cache_[i].darkEnergy) * 0.5f);
        glm::vec3 pos;
        switch (i) {
            case 0: pos = glm::vec3(0.0f, 0.0f, 0.0f); break; // Center
            case 1: pos = glm::vec3(spacing * cosf(angle), spacing * sinf(angle), 0.0f); break;
            case 2: pos = glm::vec3(spacing * cosf(angle + 2.0f * glm::pi<float>() / 3.0f), 
                                    spacing * sinf(angle + 2.0f * glm::pi<float>() / 3.0f), spacing); break;
            case 3: pos = glm::vec3(spacing * cosf(angle + 4.0f * glm::pi<float>() / 3.0f), 
                                    spacing * sinf(angle + 4.0f * glm::pi<float>() / 3.0f), -spacing); break;
            case 4: // Fifth dimension, hyper vertex
                pos = glm::vec3(0.0f, 0.0f, 0.0f) +
                      glm::vec3(0.0f, 0.0f, 1.5f * spacing * sinf(wavePhase_ + i)); break;
            default: pos = glm::vec3(spacing * cosf(angle), spacing * sinf(angle), spacing * sinf(wavePhase_ + i)); break;
        }
        pos *= zoomFactor;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, pos);
        model = glm::scale(model, glm::vec3(radius));
        model = glm::rotate(model, wavePhase_ * 0.22f + i * 0.11f, glm::normalize(glm::vec3(0.5f, 0.5f, 0.5f + 0.1f * i)));

        float dimValue = static_cast<float>(i + 1);
        float value = glm::clamp(static_cast<float>(cache_[i].observable) * heavenGlow, 0.01f, 1.0f);

        // Modulate pink with dark matter, dark energy, shimmer
        glm::vec3 modulatedColor = pinkBaseColor * (0.78f + 0.22f * static_cast<float>(cache_[i].darkMatter) * heavenGlow);
        float hyperShimmer = 0.04f * sinf(wavePhase_ * 0.9f + i);
        modulatedColor += glm::vec3(hyperShimmer, -hyperShimmer, hyperShimmer);
        modulatedColor = glm::clamp(modulatedColor, 0.0f, 1.0f);

        PushConstants pushConstants = {
            model,
            view,
            proj,
            modulatedColor,
            value,
            dimValue,
            wavePhase_,
            cycleProgress,
            static_cast<float>(cache_[i].darkMatter) * heavenGlow,
            static_cast<float>(cache_[i].darkEnergy) * heavenGlow
        };
        vkCmdPushConstants(commandBuffers_[imageIndex], navigator->pipelineLayout_,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->sphereIndices_.size()), 1, 0, 0, 0);

        std::cerr << "Mode5[D=" << i + 1 << "]: radius=" << radius << ", value=" << value
                  << ", pos=(" << pos.x << ", " << pos.y << ", " << pos.z << "), color=("
                  << modulatedColor.x << ", " << modulatedColor.y << ", " << modulatedColor.z << ")\n";
    }

    // Render interactions as glowing 5D orbital paths
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
            interactionStrength *= heavenGlow;
            interactionStrength = glm::clamp(interactionStrength, 0.01f, 2.0f);

            // Pentachoron/hyper-orbit in 5D, projected to 3D
            float orbitRadius = 1.6f + static_cast<float>(pair.distance) * 0.37f * (1.0f + static_cast<float>(pair.strength) * 0.2f);
            float angleA = wavePhase_ + pair.vertexIndex * 2.0f + pair.distance * 0.13f;
            float angleB = wavePhase_ * 0.7f + pair.vertexIndex * 0.9f + pair.distance * 0.17f;
            glm::vec3 orbitPos = glm::vec3(
                cosf(angleA) * orbitRadius * zoomFactor,
                sinf(angleA) * orbitRadius * zoomFactor,
                sinf(angleB + pair.vertexIndex) * orbitRadius * 0.5f * zoomFactor
            );
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), orbitPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(0.36f * zoomFactor * (1.0f + heavenGlow)));

            glm::vec3 interactionColor = pinkBaseColor * (0.74f + 0.26f * static_cast<float>(pair.strength) * heavenGlow);
            float hyperGlow = 0.03f * sinf(wavePhase_ * 0.8f + pair.vertexIndex);
            interactionColor += glm::vec3(hyperGlow, -hyperGlow, hyperGlow);
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
                static_cast<float>(pair.strength) * heavenGlow,
                static_cast<float>(navigator->computeDarkEnergy(pair.distance)) * heavenGlow
            };
            vkCmdPushConstants(commandBuffers_[imageIndex], navigator->pipelineLayout_,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->sphereIndices_.size()), 1, 0, 0, 0);

            std::cerr << "Mode5 Interaction[D=" << i + 1 << "]: strength=" << interactionStrength
                      << ", orbitRadius=" << orbitRadius << ", color=("
                      << interactionColor.x << ", " << interactionColor.y << ", " << interactionColor.z << ")\n";
        }
    }
}