#include "modes.hpp"
#include "main.hpp"
#include "types.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <iostream>
#include <algorithm>

static constexpr int kMaxRenderedDimensions = 9;

void renderMode4(DimensionalNavigator* navigator, uint32_t imageIndex, VkBuffer vertexBuffer_,
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
    glm::vec3 camPos = navigator->isUserCamActive_ ? navigator->userCamPos_ : glm::vec3(0.0f, 0.0f, 17.0f * zoomFactor);
    glm::mat4 proj = glm::perspective(glm::radians(50.0f), aspect, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(camPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Cycle progress for animations
    float cycleProgress = std::fmod(wavePhase_ / (4.0f * kMaxRenderedDimensions), 1.0f);
    float glowFactor = 0.8f + 0.2f * sinf(wavePhase_ * 0.6f); // Glow effect for 4D

    // Verify cache size
    if (cache_.size() < static_cast<size_t>(kMaxRenderedDimensions)) {
        std::cerr << "Warning: Cache size " << cache_.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    // Render tesseract vertices as spheres (4D hypercube has 16 vertices)
    for (size_t i = 0; i < 16; ++i) { // Tesseract has 2^4 = 16 vertices
        // Check cache for dimension data
        size_t cacheIndex = 3; // Dimension 4 (0-based index)
        if (cache_[cacheIndex].dimension != 4) {
            std::cerr << "Warning: Invalid cache for dimension 4 at index " << cacheIndex << "\n";
            continue;
        }

        // Scaling based on physical properties
        float observableScale = 1.0f + static_cast<float>(cache_[cacheIndex].observable) * 0.3f;
        float darkMatterScale = 1.0f + static_cast<float>(cache_[cacheIndex].darkMatter) * 0.5f;
        float darkEnergyScale = 1.0f + static_cast<float>(cache_[cacheIndex].darkEnergy) * 0.4f;
        float radius = 0.7f * observableScale * darkMatterScale * darkEnergyScale * (1.0f + 0.2f * sinf(wavePhase_ + i));
        radius *= zoomFactor;
        radius = glm::clamp(radius, 0.1f * zoomFactor, 9.0f * zoomFactor);

        // 4D tesseract projection to 3D
        float spacing = 2.2f * (1.0f + static_cast<float>(cache_[cacheIndex].darkEnergy) * 0.5f);
        glm::vec3 pos;
        // Compute vertex coordinates based on tesseract vertex index
        float x = (i & 1) ? 1.0f : -1.0f;
        float y = (i & 2) ? 1.0f : -1.0f;
        float z = (i & 4) ? 1.0f : -1.0f;
        float w = (i & 8) ? 1.0f : -1.0f; // 4th dimension coordinate
        // Project 4D to 3D: w influences z-offset
        pos = glm::vec3(
            spacing * x,
            spacing * y,
            spacing * (z + w * 0.5f * sinf(wavePhase_ + i))
        );
        pos *= zoomFactor;

        // Model transformation
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, pos);
        model = glm::scale(model, glm::vec3(radius));
        model = glm::rotate(model, wavePhase_ * 0.21f + i * 0.1f, glm::normalize(glm::vec3(0.6f, 0.6f, 0.6f)));

        // Color with 4D-inspired modulation
        float hyperShimmer = 0.05f * sinf(wavePhase_ * 0.85f + i);
        glm::vec3 baseColor = glm::vec3(
            1.0f - 0.2f * sinf(wavePhase_ * 0.8f + i),
            0.41f + hyperShimmer, // Pinkish tone inspired by renderMode5
            0.71f + hyperShimmer
        );
        baseColor = glm::clamp(baseColor, 0.0f, 1.0f);
        float value = glm::clamp(static_cast<float>(cache_[cacheIndex].observable) * glowFactor, 0.01f, 1.0f);

        // Push constants for rendering
        PushConstants pushConstants = {
            model,
            view,
            proj,
            baseColor,
            value,
            4.0f, // Dimension 4
            wavePhase_,
            cycleProgress,
            static_cast<float>(cache_[cacheIndex].darkMatter) * glowFactor,
            static_cast<float>(cache_[cacheIndex].darkEnergy) * glowFactor
        };
        vkCmdPushConstants(commandBuffers_[imageIndex], navigator->pipelineLayout_,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->sphereIndices_.size()), 1, 0, 0, 0);

        // Debug output
        std::cerr << "Mode4[D=4, vertex=" << i + 1 << "]: radius=" << radius << ", value=" << value
                  << ", pos=(" << pos.x << ", " << pos.y << ", " << pos.z << "), color=("
                  << baseColor.x << ", " << baseColor.y << ", " << baseColor.z << ")\n";
    }

    // Render interactions for dimension 4 as orbiting spheres
    navigator->ue_.setCurrentDimension(4);
    auto pairs = navigator->ue_.getInteractions();
    if (pairs.empty()) {
        std::cerr << "Warning: No interactions for dimension 4\n";
        // Fallback: Render a default interaction sphere
        glm::mat4 interactionModel = glm::mat4(1.0f);
        interactionModel = glm::translate(interactionModel, glm::vec3(0.0f, 0.0f, 0.0f));
        interactionModel = glm::scale(interactionModel, glm::vec3(0.35f * zoomFactor));
        glm::vec3 baseColor = glm::vec3(1.0f, 0.97f, 0.93f); // Neutral color from renderMode3
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
            // Filter interactions (optional, based on renderMode3's example)
            if (pair.vertexIndex != 1 && pair.vertexIndex != 2 && pair.vertexIndex != 3 && pair.vertexIndex != 4) continue;

            // Compute interaction strength
            float interactionStrength = static_cast<float>(
                navigator->computeInteraction(pair.vertexIndex, pair.distance) *
                std::exp(-glm::abs(navigator->ue_.getAlpha() * pair.distance)) *
                navigator->computePermeation(pair.vertexIndex) *
                glm::max(0.0f, static_cast<float>(pair.strength))
            );
            interactionStrength *= glowFactor;
            interactionStrength = glm::clamp(interactionStrength, 0.01f, 2.0f);

            // 4D-inspired orbit positioning
            float orbitRadius = 1.55f + static_cast<float>(pair.distance) * 0.45f * (1.0f + static_cast<float>(pair.strength) * 0.2f);
            float angleA = wavePhase_ + pair.vertexIndex * 2.0f + pair.distance * 0.13f;
            float angleB = wavePhase_ * 0.7f + pair.vertexIndex * 0.9f + pair.distance * 0.17f;
            glm::vec3 orbitPos = glm::vec3(
                cosf(angleA) * orbitRadius * zoomFactor,
                sinf(angleA) * orbitRadius * zoomFactor,
                cosf(angleB) * orbitRadius * 0.6f * zoomFactor * (1.0f + 0.2f * sinf(wavePhase_ + pair.vertexIndex))
            );
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), orbitPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(0.35f * zoomFactor * (1.0f + glowFactor)));

            // Interaction color
            glm::vec3 interactionColor = glm::vec3(
                1.0f - 0.17f * sinf(angleA + wavePhase_),
                0.41f + 0.15f * cosf(angleB * 1.1f),
                0.71f + 0.12f * sinf(angleB * 0.8f)
            );
            interactionColor = glm::clamp(interactionColor, 0.0f, 1.0f);

            // Push constants for interaction
            PushConstants pushConstants = {
                interactionModel,
                view,
                proj,
                interactionColor,
                interactionStrength,
                4.0f,
                wavePhase_,
                cycleProgress,
                static_cast<float>(pair.strength) * glowFactor,
                static_cast<float>(navigator->computeDarkEnergy(pair.distance)) * glowFactor
            };
            vkCmdPushConstants(commandBuffers_[imageIndex], navigator->pipelineLayout_,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->sphereIndices_.size()), 1, 0, 0, 0);

            // Debug output
            std::cerr << "Mode4 Interaction[vertex=" << pair.vertexIndex << "]: strength=" << interactionStrength
                      << ", orbitRadius=" << orbitRadius << ", pos=("
                      << orbitPos.x << ", " << orbitPos.y << ", " << orbitPos.z << "), color=("
                      << interactionColor.x << ", " << interactionColor.y << ", " << interactionColor.z << ")\n";
        }
    }
}