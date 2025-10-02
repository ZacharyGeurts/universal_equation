#include "engine/core.hpp"
#include <vulkan/vulkan.h>
#include <cstring>
#include <random>
#include <glm/gtc/random.hpp>

struct InstanceData {
    glm::vec4 baseColor;
    float alpha;
    float phase;
    glm::vec2 _pad;
};

struct UniformData {
    glm::mat4 viewInverse;
    glm::mat4 projInverse;
    glm::vec3 cameraPos;
    float time;
    glm::ivec2 imageSize;
};

void renderMode9(AMOURANTH* amouranth, [[maybe_unused]] uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer,
                 VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase,
                 const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    // Camera setup
    glm::mat4 view = glm::lookAt(
        amouranth->isUserCamActive() ? amouranth->getUserCamPos() : glm::vec3(0.0f, 0.0f, 5.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    glm::mat4 proj = glm::perspective(glm::radians(45.0f / zoomLevel), (float)width / height, 0.1f, 100.0f);
    proj[1][1] *= -1;

    // Fireworks parameters
    const uint32_t numFireworks = 20;
    const uint32_t particlesPerFirework = 60;
    const float explosionRadius = 2.0f;
    const float lifetime = 2.0f;
    static std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
    static std::uniform_real_distribution<float> dist(-2.0f, 2.0f);
    static std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);
    static std::uniform_real_distribution<float> delayDist(1.0f, 5.0f); // Random delay between 1 and 5 seconds

    // Instance buffer for ray tracing
    static std::vector<InstanceData> instanceBuffer(numFireworks * particlesPerFirework);
    uint32_t instanceIdx = 0;

    // Bind vertex and index buffers for rasterization
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    // Firework data
    static std::vector<glm::vec3> centers(numFireworks);
    static std::vector<float> startTimes(numFireworks, -1.0f);
    static std::vector<std::vector<glm::vec3>> directions(numFireworks, std::vector<glm::vec3>(particlesPerFirework));
    static std::vector<std::vector<glm::vec3>> colors(numFireworks, std::vector<glm::vec3>(particlesPerFirework));

    for (uint32_t i = 0; i < numFireworks; ++i) {
        if (startTimes[i] < 0.0f || wavePhase - startTimes[i] > lifetime) {
            centers[i] = glm::vec3(dist(rng), dist(rng), dist(rng) - 2.0f);
            // Stagger start times with a random delay between 1 and 5 seconds
            startTimes[i] = wavePhase + delayDist(rng) - 1.0f; // Offset from current wavePhase
            for (uint32_t j = 0; j < particlesPerFirework; ++j) {
                directions[i][j] = glm::sphericalRand(1.0f);
                colors[i][j] = glm::vec3(colorDist(rng), colorDist(rng), colorDist(rng));
            }
        }

        float t = (wavePhase - startTimes[i]) / lifetime;
        if (t > 1.0f) continue;

        for (uint32_t j = 0; j < particlesPerFirework; ++j) {
            glm::vec3 dir = directions[i][j];
            float radius = explosionRadius * t;
            glm::vec3 pos = centers[i] + dir * radius;
            pos.y -= 0.5f * t * t;
            glm::vec3 color = colors[i][j];
            float alpha = 1.0f - t * t;

            float brightness = 1.0f;
            if (!cache.empty()) {
                brightness += static_cast<float>(cache[0].darkEnergy) * 0.5f;
            }

            // Rasterization: Push constants
            PushConstants pc;
            pc.model = glm::translate(glm::mat4(1.0f), pos) * glm::scale(glm::mat4(1.0f), glm::vec3(0.05f * alpha));
            pc.view_proj = proj * view;
            std::memset(pc.extra, 0, sizeof(pc.extra));
            pc.extra[0] = glm::vec4(color * brightness, alpha);
            pc.extra[1] = glm::vec4(wavePhase, 0.0f, 0.0f, 0.0f);

            vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pc);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);

            // Ray tracing: Store instance data
            instanceBuffer[instanceIdx] = {glm::vec4(color * brightness, 0.0f), alpha, wavePhase, glm::vec2(0.0f)};
            instanceIdx++;
        }
    }

    // Update instance buffer (bind to binding = 3)
    // Assume instanceBuffer is copied to a VkBuffer (e.g., instanceDataBuffer) with VK_BUFFER_USAGE_STORAGE_BUFFER_BIT

    // Ray tracing dispatch (after binding ray tracing pipeline and descriptor sets)
    // vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipeline);
    // vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    // vkCmdTraceRaysKHR(commandBuffer, &raygenSBT, &missSBT, &hitSBT, &anyHitSBT, width, height, 1);
}