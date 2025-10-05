// src/modes/mode1.cpp
// AMOURANTH RTX Engine, October 2025 - RenderMode1 for 1D perspective.
// Implements rendering for a 1D line along the x-axis.
// Zachary Geurts, 2025

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include "engine/core.hpp"

void renderMode1(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer,
                 VkCommandBuffer commandBuffer, VkBuffer indexBuffer, [[maybe_unused]] float deltaTime,
                 int width, [[maybe_unused]] int height, float scale,
                 [[maybe_unused]] std::span<const DimensionData> dimData, VkPipelineLayout pipelineLayout) {
    // Bind vertex and index buffers
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // 1D projection: orthographic, focused on x-axis
    glm::mat4 proj = glm::ortho(-width / 2.0f, width / 2.0f, -1.0f, 1.0f, -1.0f, 1.0f);
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 1.0f), // Camera at (0, 0, 1)
        glm::vec3(0.0f, 0.0f, 0.0f), // Look at origin
        glm::vec3(0.0f, 1.0f, 0.0f)  // Up vector
    );

    // Model matrix (scale based on input)
    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(scale, 1.0f, 1.0f));

    // TODO: Integrate dimData when DimensionData structure is clarified
    // Example: model = glm::translate(model, glm::vec3(dimData[0].some_field, 0.0f, 0.0f));

    // Push constants
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

    // Draw geometry
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
}