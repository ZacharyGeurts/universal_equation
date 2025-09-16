#include "modes.hpp"
#include "main.hpp"
#include "types.hpp"

static constexpr int kMaxRenderedDimensions = 9;

glm::vec3 projectNDTo3D(const std::vector<double>& point, int dimension, float projectionDistance) {
    if (dimension < 3) {
        return glm::vec3(dimension > 0 ? point[0] : 0.0,
                         dimension > 1 ? point[1] : 0.0,
                         0.0);
    }
    float w = projectionDistance - (dimension > 3 ? point[dimension - 1] : 0.0);
    if (std::abs(w) < 0.001f) w = 0.001f;
    return glm::vec3(point[0] / w, point[1] / w, point[2] / w);
}

void renderMode4(DimensionalNavigator* navigator, uint32_t imageIndex, VkBuffer vertexBuffer_,
                 std::vector<VkCommandBuffer>& commandBuffers_, VkBuffer indexBuffer_,
                 float zoomLevel_, int width_, int height_, float wavePhase_,
                 std::vector<DimensionData>& cache_) {
    VkBuffer vertexBuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffers_[imageIndex], indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

    float zoomFactor = glm::max(zoomLevel_, 0.01f);
    float aspect = static_cast<float>(width_) / glm::max(1.0f, static_cast<float>(height_));
    glm::vec3 camPos = navigator->isUserCamActive_ ? navigator->userCamPos_ : glm::vec3(0.0f, 0.0f, 5.0f * zoomFactor);
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

        float angleXY = wavePhase_ * 0.1f;
        float angleZW = wavePhase_ * 0.05f;
        glm::mat4 rotation4D = glm::mat4(1.0f);
        rotation4D[0][0] = cosf(angleXY); rotation4D[0][1] = -sinf(angleXY);
        rotation4D[1][0] = sinf(angleXY); rotation4D[1][1] = cosf(angleXY);
        rotation4D[2][2] = cosf(angleZW); rotation4D[2][3] = -sinf(angleZW);
        rotation4D[3][2] = sinf(angleZW); rotation4D[3][3] = cosf(angleZW);

        float geometryScale = (1.0f + static_cast<float>(cache_[i].observable) * 0.5f) * zoomFactor;
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(geometryScale));

        glm::vec3 baseColor = glm::vec3(
            0.5f + 0.5f * sinf(wavePhase_ * 0.5f),
            0.5f + 0.5f * cosf(wavePhase_ * 0.5f),
            0.5f + 0.5f * sinf(wavePhase_ * 0.7f)
        );

        PushConstants pushConstants = {
            model,
            view,
            proj,
            baseColor,
            static_cast<float>(cache_[i].darkMatter),
            4.0f,
            wavePhase_,
            cycleProgress,
            static_cast<float>(cache_[i].darkEnergy),
            static_cast<float>(cache_[i].potential)
        };
        vkCmdPushConstants(commandBuffers_[imageIndex], navigator->pipelineLayout_,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->sphereIndices_.size()), 1, 0, 0, 0);

        std::cerr << "Mode4[D=4]: geometryScale=" << geometryScale
                  << ", darkMatter=" << cache_[i].darkMatter
                  << ", pos=(0, 0, 0), color=(" << baseColor.x << ", " << baseColor.y << ", " << baseColor.z << ")\n";
    }

    navigator->ue_.setCurrentDimension(4);
    auto pairs = navigator->ue_.getInteractions();
    if (pairs.empty()) {
        std::cerr << "Warning: No interactions for dimension 4\n";
        glm::mat4 interactionModel = glm::mat4(1.0f);
        interactionModel = glm::translate(interactionModel, glm::vec3(0.0f, 0.0f, 0.0f));
        interactionModel = glm::scale(interactionModel, glm::vec3(0.5f * zoomFactor));
        glm::vec3 baseColor = glm::vec3(0.5f, 0.5f, 0.5f);
        PushConstants pushConstants = {
            interactionModel,
            view,
            proj,
            baseColor,
            0.5f,
            4.0f,
            wavePhase_,
            cycleProgress,
            0.0f,
            0.0f
        };
        vkCmdPushConstants(commandBuffers_[imageIndex], navigator->pipelineLayout_,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->sphereIndices_.size()), 1, 0, 0, 0);
    } else {
        for (const auto& pair : pairs) {
            std::vector<double> vertex(4, 0.0);
            int vertexIndex = pair.vertexIndex % (1 << 4);
            for (int j = 0; j < 4; ++j) {
                vertex[j] = (vertexIndex & (1 << j)) ? 1.0 : -1.0;
            }
            glm::vec4 point4D(vertex[0], vertex[1], vertex[2], vertex[3]);
            float angleXY = wavePhase_ * 0.1f;
            float angleZW = wavePhase_ * 0.05f;
            glm::mat4 rotation4D = glm::mat4(1.0f);
            rotation4D[0][0] = cosf(angleXY); rotation4D[0][1] = -sinf(angleXY);
            rotation4D[1][0] = sinf(angleXY); rotation4D[1][1] = cosf(angleXY);
            rotation4D[2][2] = cosf(angleZW); rotation4D[2][3] = -sinf(angleZW);
            rotation4D[3][2] = sinf(angleZW); rotation4D[3][3] = cosf(angleZW);
            point4D = rotation4D * point4D;
            glm::vec3 orbitPos = projectNDTo3D(vertex, 4, 5.0f * zoomFactor);
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), orbitPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(0.3f * zoomFactor * (1.0f + 0.2f * pair.strength)));

            glm::vec3 baseColor = glm::vec3(
                0.5f + 0.5f * sinf(wavePhase_ * 0.5f + pair.vertexIndex),
                0.5f + 0.5f * cosf(wavePhase_ * 0.5f + pair.vertexIndex),
                0.5f
            );
            PushConstants pushConstants = {
                interactionModel,
                view,
                proj,
                baseColor,
                static_cast<float>(pair.strength),
                4.0f,
                wavePhase_,
                cycleProgress,
                static_cast<float>(navigator->computeDarkEnergy(pair.distance)),
                0.0f
            };
            vkCmdPushConstants(commandBuffers_[imageIndex], navigator->pipelineLayout_,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(navigator->sphereIndices_.size()), 1, 0, 0, 0);

            std::cerr << "Mode4 Interaction[vertex=" << pair.vertexIndex << "]: strength=" << pair.strength
                      << ", pos=(" << orbitPos.x << ", " << orbitPos.y << ", " << orbitPos.z << ")\n";
        }
    }
}