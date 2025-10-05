// src/modes/mode6.cpp
// AMOURANTH RTX Engine, October 2025 - RenderMode6 for 6D perspective.
// Implements rendering for a 3D slice of a 6D hypercube.
// Zachary Geurts, 2025

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include "engine/core.hpp"

void renderMode6(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer,
                 VkCommandBuffer commandBuffer, VkBuffer indexBuffer, [[maybe_unused]] float deltaTime,
                 int width, int height, float scale, [[maybe_unused]] std::span<const DimensionData> dimData,
                 VkPipelineLayout pipelineLayout) {
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    glm::mat4 proj = glm::scale(glm::mat4(1.0f), glm::vec3(0.7f, 0.7f, 0.7f)) *
                     glm::ortho(-width / 2.0f, width / 2.0f, -height / 2.0f, height / 2.0f, -10.0f, 10.0f);
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 11.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(scale * 0.7f, scale * 0.7f, scale * 0.7f));

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