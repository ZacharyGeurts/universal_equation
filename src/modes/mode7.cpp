#include "engine/core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <stdexcept>

void renderMode7(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet) {
    if (!amouranth || !commandBuffer || !pipelineLayout) {
        throw std::runtime_error("renderMode7: Invalid AMOURANTH, commandBuffer, or pipelineLayout");
    }
    if (cache.size() < 7) {
        throw std::runtime_error("renderMode7: Insufficient cache data for dimension 7");
    }

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    PushConstants pushConstants = {};
    float observable = cache[6].observable;
    float animatedScale = observable * 1.1f * (1.0f + 0.4f * (std::sin(wavePhase * 2.2f) + std::cos(wavePhase * 1.3f)));
    pushConstants.model = glm::scale(glm::mat4(1.0f), glm::vec3(std::max(0.1f, animatedScale)));
    pushConstants.model = glm::rotate(pushConstants.model, wavePhase * 0.7f, glm::vec3(1.0f, 0.0f, 1.0f));

    glm::vec3 cameraPos = amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, 1.5f * zoomLevel);
    glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(75.0f / std::max(0.1f, zoomLevel)), static_cast<float>(width) / height, 0.1f, 100.0f);
    proj[1][1] *= -1;
    pushConstants.view_proj = proj * view;

    pushConstants.extra[0].x = observable;
    pushConstants.extra[1].x = std::sin(wavePhase * 1.6f) * 0.8f;
    pushConstants.extra[2].x = std::cos(wavePhase * 1.4f) * 0.7f;
    pushConstants.extra[3].x = std::sin(wavePhase * 1.1f) * 0.5f;
    pushConstants.extra[4].x = std::cos(wavePhase * 0.8f) * 0.4f;
    pushConstants.extra[5].x = std::sin(wavePhase * 0.5f) * 0.3f;
    pushConstants.extra[6].x = std::cos(wavePhase * 0.2f) * 0.2f;
    pushConstants.extra[7] = glm::vec4(0.0f);

    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);

    if (amouranth->getDebug()) {
        std::cout << std::format("[DEBUG] Rendering frame {} for dimension 7 with observable {}\n", imageIndex, observable);
    }
    auto indices = amouranth->getSphereIndices();
    if (indices.empty()) {
        throw std::runtime_error("renderMode7: Sphere indices are empty");
    }
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
}