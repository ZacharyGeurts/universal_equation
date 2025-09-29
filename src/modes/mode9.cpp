#include "core.hpp"
#include <vulkan/vulkan.h>
#include <cstring>

void renderMode9(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, [[maybe_unused]] float wavePhase,
                 [[maybe_unused]] const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    // Camera setup
    glm::mat4 view = glm::lookAt(
        amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, 3.0f),
        glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f)
    );
    glm::mat4 proj = glm::perspective(glm::radians(45.0f / zoomLevel), (float)width / height, 0.1f, 100.0f);
    proj[1][1] *= -1; // Flip Y for Vulkan

    // Use PushConstants from core.hpp (256 bytes)
    PushConstants pc;
    pc.model = glm::mat4(1.0f);
    pc.view_proj = proj * view;
    std::memset(pc.extra, 0, sizeof(pc.extra)); // Zero out extra data

    // Bind vertex and index buffers
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Push constants
    vkCmdPushConstants(
        commandBuffer,
        pipelineLayout,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0,
        sizeof(PushConstants), // 256 bytes
        &pc
    );

    // Draw
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
}