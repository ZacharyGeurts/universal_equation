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

void renderMode2(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 std::span<const DimensionData> cache, VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet) {
    if (!amouranth || !commandBuffer || !pipelineLayout || !vertexBuffer || !indexBuffer) {
        throw std::runtime_error("renderMode2: Invalid parameters");
    }
    if (cache.empty()) {
        throw std::runtime_error("renderMode2: Cache is empty");
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

    glm::vec3 cameraPos = amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, 7.0f * zoomLevel);
    glm::mat4 view = glm::lookAt(cameraPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f / std::max(0.1f, zoomLevel)), static_cast<float>(width) / height, 0.1f, 100.0f);
    proj[1][1] *= -1; // Vulkan Y-axis flip

    // Dimensional scaling factors
    float baseScale = std::max(0.1f, observable * 0.5f);
    float scaleFactor1 = baseScale * 1.0f; // Rainbow sphere
    float scaleFactor2 = baseScale * 0.9f; // Swirling sphere
    float scaleFactor3 = baseScale * 0.8f; // Pulsating bands sphere

    PushConstants pushConstants = {};
    auto indices = amouranth->getSphereIndices();
    if (indices.empty()) {
        throw std::runtime_error("renderMode2: Sphere indices are empty");
    }

    // First sphere (mode 1: rainbow, left)
    pushConstants.model = glm::translate(glm::mat4(1.0f), glm::vec3(-1.6f, 0.0f, 0.0f)) *
                         glm::scale(glm::mat4(1.0f), glm::vec3(scaleFactor1));
    pushConstants.view_proj = proj * view;
    pushConstants.extra[0].x = observable;
    pushConstants.extra[0].y = 0.7f; // Alpha for transparency
    pushConstants.extra[1].x = wavePhase;
    pushConstants.extra[2].x = 1.0f; // Mode 1 (rainbow)
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    // Second sphere (mode 2: swirling, center)
    pushConstants.model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f)) *
                         glm::scale(glm::mat4(1.0f), glm::vec3(scaleFactor2));
    pushConstants.view_proj = proj * view;
    pushConstants.extra[0].x = observable;
    pushConstants.extra[0].y = 0.65f; // Slightly different alpha
    pushConstants.extra[1].x = wavePhase;
    pushConstants.extra[2].x = 2.0f; // Mode 2 (swirling)
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 1);

    // Third sphere (mode 3: pulsating bands, right)
    pushConstants.model = glm::translate(glm::mat4(1.0f), glm::vec3(1.6f, 0.0f, 0.0f)) *
                         glm::scale(glm::mat4(1.0f), glm::vec3(scaleFactor3));
    pushConstants.view_proj = proj * view;
    pushConstants.extra[0].x = observable;
    pushConstants.extra[0].y = 0.6f; // Different alpha for distinction
    pushConstants.extra[1].x = wavePhase;
    pushConstants.extra[2].x = 3.0f; // Mode 3 (pulsating bands)
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 2);

    if (amouranth->getDebug()) {
        std::cout << std::format("[DEBUG] Rendering frame {} for mode 2 with observable {}, wavePhase {}, scales [{}, {}, {}]\n",
                                 imageIndex, observable, wavePhase, scaleFactor1, scaleFactor2, scaleFactor3);
    }
}