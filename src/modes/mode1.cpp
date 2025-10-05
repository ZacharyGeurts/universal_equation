#include "engine/core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <span>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <format>
#include <iostream>

void renderMode1(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet) {
    if (!amouranth || !commandBuffer || !pipelineLayout || !vertexBuffer || !indexBuffer) {
        throw std::runtime_error("renderMode1: Invalid parameters");
    }
    if (cache.empty()) {
        throw std::runtime_error("renderMode1: Cache is empty");
    }

    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    float observable = static_cast<float>(cache[0].observable);
    if (!std::isfinite(observable) || observable < 0.0f) {
        std::cerr << "[WARNING] Invalid observable: " << observable << ", using 1.0\n";
        observable = 1.0f;
    }

    glm::vec3 cameraPos = amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, 5.0f * zoomLevel);
    glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f / std::max(0.1f, zoomLevel)), static_cast<float>(width) / height, 0.1f, 100.0f);
    proj[1][1] *= -1; // Vulkan Y-axis flip

    // Dimensional scaling factor based on observable
    float baseScale = std::max(0.1f, observable * 0.5f);
    float scaleFactor1 = baseScale * 1.0f; // First sphere scale
    float scaleFactor2 = baseScale * 0.8f; // Second sphere slightly smaller

    // First sphere (mode 1: rainbow)
    PushConstants pushConstants = {};
    pushConstants.model = glm::translate(glm::mat4(1.0f), glm::vec3(-0.8f, 0.0f, 0.0f)) *
                         glm::scale(glm::mat4(1.0f), glm::vec3(scaleFactor1));
    pushConstants.view_proj = proj * view;
    pushConstants.extra[0].x = observable;
    pushConstants.extra[0].y = 0.7f; // Alpha for transparency
    pushConstants.extra[1].x = wavePhase;
    pushConstants.extra[2].x = 1.0f; // Mode indicator (1 for rainbow)

    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);

    auto indices = amouranth->getSphereIndices();
    if (indices.empty()) {
        throw std::runtime_error("renderMode1: Sphere indices are empty");
    }
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    // Second sphere (mode 2: swirling)
    pushConstants.model = glm::translate(glm::mat4(1.0f), glm::vec3(0.8f, 0.0f, 0.0f)) *
                         glm::scale(glm::mat4(1.0f), glm::vec3(scaleFactor2));
    pushConstants.view_proj = proj * view;
    pushConstants.extra[0].x = observable;
    pushConstants.extra[0].y = 0.6f; // Slightly different alpha
    pushConstants.extra[1].x = wavePhase;
    pushConstants.extra[2].x = 2.0f; // Mode indicator (2 for swirling)

    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 1);

    if (amouranth->getDebug()) {
        std::cout << std::format("[DEBUG] Rendering frame {} for mode 1 with observable {}, wavePhase {}, scales [{}, {}]\n",
                                 imageIndex, observable, wavePhase, scaleFactor1, scaleFactor2);
    }
}