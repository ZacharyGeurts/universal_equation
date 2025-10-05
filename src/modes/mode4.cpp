#include "engine/core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <stdexcept>

void renderMode4(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet) {
    if (!amouranth || !commandBuffer || !pipelineLayout) {
        throw std::runtime_error("renderMode4: Invalid AMOURANTH, commandBuffer, or pipelineLayout");
    }
    if (cache.size() < 4) {
        throw std::runtime_error("renderMode4: Insufficient cache data for dimension 4");
    }

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    PushConstants pushConstants = {};
    float observable = cache[3].observable;
    float animatedScale = observable * 0.8f * (1.0f + 0.25f * (std::sin(wavePhase * 1.5f) + std::cos(wavePhase * 0.7f)));
    pushConstants.model = glm::scale(glm::mat4(1.0f), glm::vec3(std::max(0.1f, animatedScale)));
    pushConstants.model = glm::rotate(pushConstants.model, wavePhase * 0.4f, glm::vec3(0.5f, 1.0f, 0.5f));

    glm::vec3 cameraPos = amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, 3.0f * zoomLevel);
    glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(60.0f / std::max(0.1f, zoomLevel)), static_cast<float>(width) / height, 0.1f, 100.0f);
    proj[1][1] *= -1;
    pushConstants.view_proj = proj * view;

    pushConstants.extra[0].x = observable;
    pushConstants.extra[1].x = std::sin(wavePhase * 1.0f) * 0.5f;
    pushConstants.extra[2].x = std::cos(wavePhase * 0.9f) * 0.4f;
    pushConstants.extra[3].x = std::sin(wavePhase * 0.5f) * 0.2f;
    for (int i = 4; i < 8; ++i) {
        pushConstants.extra[i] = glm::vec4(0.0f);
    }

    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);

    if (amouranth->getDebug()) {
        std::cout << std::format("[DEBUG] Rendering frame {} for dimension 4 with observable {}\n", imageIndex, observable);
    }
    auto indices = amouranth->getSphereIndices();
    if (indices.empty()) {
        throw std::runtime_error("renderMode4: Sphere indices are empty");
    }
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
}