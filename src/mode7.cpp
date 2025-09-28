// renderMode7.cpp
// AMOURANTH RTX Engine - Render Mode 7 - September 2025
// Renders a sphere modulated by UniversalEquation::EnergyResult for mode 7 (7D projection).
// Uses observable for scale, darkEnergy for position, and energy values for color.
// Fixed: Zoomed out 200% by tripling the view translation distance to -9.0f.
// Compatible with existing Vulkan pipeline layout and extended PushConstants.
// Zachary Geurts, 2025

#include "core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

void renderMode7(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Use the latest cache entry for mode 7
    DimensionData energyData = cache.empty() ? DimensionData{7, 0.0, 0.0, 0.0, 0.0} : cache.back();

    // PushConstants with energy values
    struct PushConstants {
        glm::mat4 model;
        glm::mat4 view_proj;
        glm::vec4 energy; // observable, potential, darkMatter, darkEnergy
    } pushConstants;

    // Model matrix: scale based on observable, translate based on darkEnergy
    pushConstants.model = glm::mat4(1.0f);
    float scale = 1.0f + 0.1f * sin(wavePhase) + 0.5f * static_cast<float>(energyData.observable) * 0.7f; // Mode 7
    pushConstants.model = glm::scale(pushConstants.model, glm::vec3(scale));
    pushConstants.model = glm::translate(pushConstants.model, glm::vec3(0.0f, 0.0f, 0.5f * static_cast<float>(energyData.darkEnergy) * 0.7f));

    // View-projection matrix: zoomed out 200% by tripling z-translation
    pushConstants.view_proj = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / height, 0.1f, 100.0f);
    pushConstants.view_proj = glm::translate(pushConstants.view_proj, glm::vec3(0.0f, 0.0f, -9.0f * zoomLevel)); // Tripled from -3.0f

    // Pass energy values to fragment shader
    pushConstants.energy = glm::vec4(
        static_cast<float>(energyData.observable),
        static_cast<float>(energyData.potential),
        static_cast<float>(energyData.darkMatter),
        static_cast<float>(energyData.darkEnergy)
    );

    // Push constants
    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 
                       0, sizeof(PushConstants), &pushConstants);

    uint32_t indexCount = amouranth->getSphereIndices().size();
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
}