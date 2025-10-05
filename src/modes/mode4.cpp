// src/modes/mode4.cpp
// AMOURANTH RTX Engine, October 2025 - RenderMode4 for 4D perspective.
// Implements rendering for a 3D slice of a 4D tesseract.
// Zachary Geurts, 2025

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include "engine/core.hpp"

void renderMode4(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer,
                 VkCommandBuffer commandBuffer, VkBuffer indexBuffer, [[maybe_unused]] float deltaTime,
                 int width, int height, float scale, [[maybe_unused]] std::span<const DimensionData> dimData,
                 VkPipelineLayout pipelineLayout) {
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // 4D projection: perspective with w=0
    glm::mat4 proj4D = glm::mat4(1.0f);
    proj4D[3][3] = 0.0f;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), width / (float)height, 0.1f, 100.0f) * proj4D;
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 7.0f), // Camera farther for 4D
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(scale * 0.9f, scale * 0.9f, scale * 0.9f));

    // TODO: Integrate dimData when DimensionData structure is clarified

    struct PushConstants {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
    } push;
    push.model = model;
    push.view = view;
    push.proj = proj;

    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
                       0, sizeof(PushConstants), &push);

    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
}