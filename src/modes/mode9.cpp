#include "engine/core.hpp"
#include <vulkan/vulkan.h>
#include <cstring>
#include <random>
#include <glm/gtc/random.hpp>
#include <cmath>

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

enum class FireworkType {
    SPHERICAL,
    RING,
    FOUNTAIN,
    COMET,
    WILLOW,
    PALM,
    PEONY,
    CHRYSANTHEMUM
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
    const uint32_t numFireworks = 30;
    const uint32_t particlesPerFirework = 80;
    const float explosionRadius = 2.5f;
    const float lifetime = 2.0f;
    static std::mt19937 rng(static_cast<unsigned>(std::time(nullptr)));
    static std::uniform_real_distribution<float> dist(-2.0f, 2.0f);
    static std::uniform_real_distribution<float> colorDist(0.0f, 1.0f);
    static std::uniform_real_distribution<float> delayDist(0.5f, 4.0f);
    static std::uniform_int_distribution<int> typeDist(0, 7); // 8 types

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
    static std::vector<FireworkType> types(numFireworks);
    static std::vector<std::vector<glm::vec3>> directions(numFireworks, std::vector<glm::vec3>(particlesPerFirework));
    static std::vector<std::vector<glm::vec3>> colors(numFireworks, std::vector<glm::vec3>(particlesPerFirework));
    static std::vector<glm::vec3> ringNormals(numFireworks);
    static std::vector<glm::vec3> ringRights(numFireworks);
    static std::vector<glm::vec3> ringUps(numFireworks);
    static std::vector<glm::vec3> fountainDir(numFireworks);
    static std::vector<glm::vec3> cometDir(numFireworks);
    static std::uniform_real_distribution<float> angleDist(0.0f, 0.5f); // Spread angle
    static std::uniform_real_distribution<float> speedDist(0.5f, 1.5f); // Vary speeds for dynamics

    const float pi = 3.1415926535f;
    static std::uniform_real_distribution<float> phiDist(0.0f, 2.0f * pi);
    static std::uniform_real_distribution<float> thetaDist(0.0f, pi);

    for (uint32_t i = 0; i < numFireworks; ++i) {
        if (startTimes[i] < 0.0f || wavePhase - startTimes[i] > lifetime) {
            centers[i] = glm::vec3(dist(rng), dist(rng), dist(rng) - 3.0f);
            startTimes[i] = wavePhase + delayDist(rng);
            types[i] = static_cast<FireworkType>(typeDist(rng));

            // Color scheme based on type
            glm::vec3 baseColor;
            switch (types[i]) {
                case FireworkType::SPHERICAL: baseColor = glm::vec3(1.0f, 0.5f, 0.0f); break; // Orange
                case FireworkType::RING: baseColor = glm::vec3(0.0f, 1.0f, 1.0f); break; // Cyan
                case FireworkType::FOUNTAIN: baseColor = glm::vec3(1.0f, 0.0f, 0.0f); break; // Red
                case FireworkType::COMET: baseColor = glm::vec3(1.0f, 1.0f, 0.0f); break; // Yellow
                case FireworkType::WILLOW: baseColor = glm::vec3(0.0f, 0.5f, 1.0f); break; // Blue
                case FireworkType::PALM: baseColor = glm::vec3(0.0f, 1.0f, 0.0f); break; // Green
                case FireworkType::PEONY: baseColor = glm::vec3(1.0f, 0.0f, 1.0f); break; // Magenta
                case FireworkType::CHRYSANTHEMUM: baseColor = glm::vec3(1.0f, 1.0f, 1.0f); break; // White
                default: baseColor = glm::vec3(1.0f); break;
            }

            // Setup type-specific data
            switch (types[i]) {
                case FireworkType::RING: {
                    ringNormals[i] = glm::sphericalRand(1.0f);
                    glm::vec3 arbitrary = glm::abs(ringNormals[i].y) < 0.9f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
                    ringRights[i] = glm::normalize(glm::cross(ringNormals[i], arbitrary));
                    ringUps[i] = glm::cross(ringNormals[i], ringRights[i]);
                    break;
                }
                case FireworkType::FOUNTAIN: {
                    fountainDir[i] = glm::vec3(0.0f, 1.0f, 0.0f) + glm::sphericalRand(angleDist(rng));
                    fountainDir[i] = glm::normalize(fountainDir[i]);
                    break;
                }
                case FireworkType::COMET: {
                    cometDir[i] = glm::sphericalRand(1.0f);
                    break;
                }
                case FireworkType::WILLOW:
                case FireworkType::PALM: {
                    // Similar to comet but with droop or spread
                    cometDir[i] = glm::sphericalRand(1.0f); // Reuse for direction
                    break;
                }
                default: break;
            }

            for (uint32_t j = 0; j < particlesPerFirework; ++j) {
                float speed = speedDist(rng);
                glm::vec3 dir;
                switch (types[i]) {
                    case FireworkType::SPHERICAL: {
                        // Uniform sphere (corrected for uniformity)
                        float u = static_cast<float>(j) / particlesPerFirework;
                        float v = static_cast<float>(j + 1) / particlesPerFirework;
                        float theta_sph = acosf(1.0f - 2.0f * u);
                        float phi = 2.0f * pi * v;
                        dir = glm::vec3(sinf(theta_sph) * cosf(phi), sinf(theta_sph) * sinf(phi), cosf(theta_sph));
                        dir *= speed;
                        break;
                    }
                    case FireworkType::RING: {
                        float theta = 2.0f * pi * static_cast<float>(j) / particlesPerFirework;
                        dir = cosf(theta) * ringRights[i] + sinf(theta) * ringUps[i];
                        dir *= speed;
                        break;
                    }
                    case FireworkType::FOUNTAIN: {
                        float phi = phiDist(rng);
                        float spread = angleDist(rng) * 0.5f;
                        float theta_f = asinf(spread * sinf(phi)) + asinf(spread * cosf(phi)); // Cone spread
                        dir = glm::vec3(sinf(theta_f) * cosf(phi), cosf(theta_f), sinf(theta_f) * sinf(phi));
                        dir = glm::normalize(dir + fountainDir[i] * 0.1f); // Bias upward
                        dir *= speed;
                        break;
                    }
                    case FireworkType::COMET: {
                        float offset = static_cast<float>(j) / particlesPerFirework - 0.5f;
                        dir = cometDir[i] + glm::vec3(offset * 0.1f, offset * 0.05f, offset * 0.1f); // Linear trail with slight spread
                        dir = glm::normalize(dir) * speed;
                        break;
                    }
                    case FireworkType::WILLOW: {
                        // Drooping: initial upward, but with heavy gravity simulation per particle
                        float theta_w = thetaDist(rng) * 0.3f; // Mostly upward
                        float phi_w = phiDist(rng);
                        dir = glm::vec3(sinf(theta_w) * cosf(phi_w), cosf(theta_w), sinf(theta_w) * sinf(phi_w));
                        dir *= speed;
                        break;
                    }
                    case FireworkType::PALM: {
                        // Palm: few thick branches
                        int branch = j % 5; // 5 branches
                        float angle = (branch - 2) * 0.4f;
                        dir = glm::vec3(-sinf(angle), cosf(angle), 0.0f);
                        dir *= speed;
                        break;
                    }
                    case FireworkType::PEONY: {
                        // Peony: dense spherical with color variation
                        dir = glm::sphericalRand(1.0f) * speed;
                        break;
                    }
                    case FireworkType::CHRYSANTHEMUM: {
                        // Chrysanthemum: spherical with trails (simulated by varying speed)
                        dir = glm::sphericalRand(1.0f) * speedDist(rng);
                        break;
                    }
                    default: dir = glm::sphericalRand(1.0f); break;
                }
                directions[i][j] = dir;

                // Vary colors around base
                colors[i][j] = baseColor + glm::vec3(colorDist(rng) * 0.3f - 0.15f);
                colors[i][j] = glm::clamp(colors[i][j], 0.0f, 1.0f);
            }
        }

        float t = glm::clamp((wavePhase - startTimes[i]) / lifetime, 0.0f, 1.0f);
        if (t >= 1.0f) continue;

        float burstT = t * t; // Ease in
        float fadeT = 1.0f - t * t * 0.5f; // Slower fade

        for (uint32_t j = 0; j < particlesPerFirework; ++j) {
            glm::vec3 dir = directions[i][j];
            glm::vec3 initialVel = dir;
            float speed = glm::length(initialVel);

            // Position: explode without gravity
            glm::vec3 pos = centers[i] + initialVel * burstT * explosionRadius / speed;

            glm::vec3 color = colors[i][j];
            float alpha = fadeT * (1.0f - t * 0.3f);

            // Twinkle effect
            float twinkle = 1.0f + 0.5f * sinf(wavePhase * 10.0f + static_cast<float>(j));

            float brightness = twinkle;
            if (!cache.empty()) {
                brightness += static_cast<float>(cache[0].darkEnergy) * 0.5f;
            }

            // Rasterization: Push constants
            PushConstants pc;
            pc.model = glm::translate(glm::mat4(1.0f), pos) * glm::scale(glm::mat4(1.0f), glm::vec3(0.03f * alpha * brightness));
            pc.view_proj = proj * view;
            std::memset(pc.extra, 0, sizeof(pc.extra));
            pc.extra[0] = glm::vec4(color * brightness, alpha);
            pc.extra[1] = glm::vec4(wavePhase, static_cast<float>(static_cast<int>(types[i])), 0.0f, 0.0f);

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