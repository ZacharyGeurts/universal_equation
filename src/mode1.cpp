#include "core.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <vector>

void renderMode1(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    const glm::vec4 kDefaultColor = {1.0f, 0.0f, 0.0f, 1.0f}; // Red as default
    const float cycleProgress = 0.0f;

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / height, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(amouranth->getUserCamPos(), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 viewProj = proj * view;

    glm::mat4 model = glm::mat4(1.0f);
    float scale = 1.0f + 0.1f * std::sin(wavePhase) * zoomLevel;
    model = glm::scale(model, glm::vec3(scale));

    glm::vec4 baseColor = kDefaultColor;
    float value = cache.empty() ? 0.4f : static_cast<float>(cache[0].observable);

    PushConstants constants = {
        viewProj,
        amouranth->getUserCamPos(),
        wavePhase,
        cycleProgress,
        zoomLevel,
        value,
        cache.empty() ? 0.0f : static_cast<float>(cache[0].darkMatter),
        cache.empty() ? 0.0f : static_cast<float>(cache[0].darkEnergy),
        baseColor, // Use baseColor for primary draw
        {0.0f, 0.0f, 0.0f}
    };

    vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &constants);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, {});
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);

    if (cache.empty()) {
        PushConstants fallbackPush = {
            viewProj,
            amouranth->getUserCamPos(),
            wavePhase,
            cycleProgress,
            zoomLevel,
            0.4f,
            0.0f,
            0.0f,
            kDefaultColor,
            {0.0f, 0.0f, 0.0f}
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &fallbackPush);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
    }

    auto interactions = amouranth->getInteractions();
    auto renderInstance = [&](const UniversalEquation::DimensionInteraction& interaction, float strengthMod, const glm::vec4& iColor) {
        glm::mat4 iModel = glm::mat4(1.0f);
        float iScale = scale * (1.0f + 0.05f * static_cast<float>(interaction.strength));
        iModel = glm::scale(iModel, glm::vec3(iScale));
        iModel = glm::translate(iModel, glm::vec3(static_cast<float>(interaction.vertexIndex) * 0.5f, 0.0f, 0.0f));

        PushConstants iPush = {
            viewProj,
            amouranth->getUserCamPos(),
            wavePhase,
            cycleProgress,
            zoomLevel,
            static_cast<float>(interaction.strength) * strengthMod,
            cache.empty() ? 0.0f : static_cast<float>(cache[0].darkMatter),
            cache.empty() ? 0.0f : static_cast<float>(cache[0].darkEnergy),
            iColor,
            {0.0f, 0.0f, 0.0f}
        };

        vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &iPush);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
    };

    const glm::vec4 colors[] = {
        {1.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f},
        {1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f, 1.0f},
        {1.0f, 0.5f, 0.0f, 1.0f}, {0.5f, 0.5f, 0.5f, 1.0f}
    };

    for (size_t i = 0; i < interactions.size() && i < 8; ++i) {
        renderInstance(interactions[i], 0.4f + 0.1f * i, colors[i % 8]);
    }
}