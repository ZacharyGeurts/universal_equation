#include "core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <vulkan/vulkan.h>
#include <iostream>
#include <cmath>
#include <random> // For subtle randomness

static constexpr int kMaxRenderedDimensions = 9;

void renderMode1(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
    (void)imageIndex;

    VkBuffer vertexBuffers[] = {vertexBuffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

    if (amouranth->getSphereIndices().empty()) {
        std::cerr << "Warning: Sphere indices empty\n";
        return;
    }

    float zoomFactor = glm::max(zoomLevel, 0.01f);
    float aspect = static_cast<float>(width) / glm::max(1.0f, static_cast<float>(height));
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 1000.0f);

    // Dynamic camera for 1D: Orbit slowly around the action for immersion
    glm::vec3 camPos(0.0f, 3.0f * zoomFactor, 6.0f * zoomFactor);
    if (amouranth->isUserCamActive()) {
        camPos = amouranth->getUserCamPos();
    }
    glm::vec3 camTarget(0.0f, 0.0f, 0.0f);
    glm::vec3 camUp(0.0f, 1.0f, 0.0f);
    // Gentle orbit based on wavePhase
    float orbitAngle = wavePhase * 0.2f;
    camPos.x = 4.0f * zoomFactor * sinf(orbitAngle);
    camPos.z = 4.0f * zoomFactor * cosf(orbitAngle);
    glm::mat4 view = glm::lookAt(camPos, camTarget, camUp);

    float cycleProgress = std::fmod(wavePhase / (2.0f * kMaxRenderedDimensions), 1.0f);

    if (cache.size() < kMaxRenderedDimensions) {
        std::cerr << "Warning: Cache size " << cache.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    size_t i = 0; // Dimension 1
    if (cache[i].dimension != 1) {
        std::cerr << "Warning: Invalid cache for dimension 1\n";
        return;
    }

    // Enhanced oscillation: Tie to multiple cache values for richer motion
    float osc = 1.0f + 0.3f * sinf(wavePhase * 1.5f + static_cast<float>(cache[i].darkEnergy) * 2.0f);
    float pulse = 0.5f + 0.5f * sinf(wavePhase * 2.0f + static_cast<float>(cache[i].potential) * 3.0f);
    float value = static_cast<float>(cache[i].observable * osc * pulse);
    value = glm::clamp(value, 0.01f, 2.0f); // Wider range for drama

    // 1D emphasis: Linear oscillation along x-axis, like a waveform
    float linearPos = 3.0f * value * sinf(wavePhase * 1.2f + cycleProgress * glm::two_pi<float>());
    float angle = wavePhase * 0.8f + i * glm::two_pi<float>() / kMaxRenderedDimensions;
    float scaleFactor = 1.0f + static_cast<float>(cache[i].observable) * 0.5f * pulse;
    float radius = 1.5f * scaleFactor; // Slightly larger base
    glm::vec3 pos = glm::vec3(
        linearPos + radius * cosf(angle),
        radius * sinf(angle) * 0.5f, // Subtle y variation
        0.0f * cosf(wavePhase * 0.5f) // Hint of z-depth
    );
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos);
    // Dynamic scaling: Stretch along x for 1D feel
    float stretchX = 1.5f * pulse;
    model = glm::scale(model, glm::vec3(stretchX * 0.4f * zoomFactor * osc * scaleFactor,
                                        0.3f * zoomFactor * osc * scaleFactor,
                                        0.3f * zoomFactor * osc * scaleFactor));

    // Vibrant, reactive colors: Hue shift with darkMatter, saturation with energy
    float hueShift = static_cast<float>(cache[i].darkMatter) * 2.0f * glm::pi<float>();
    glm::vec3 baseColor = glm::vec3(
        0.5f + 0.5f * cosf(wavePhase * 1.0f + hueShift),
        0.3f + 0.4f * sinf(wavePhase * 0.8f + cycleProgress * glm::pi<float>()),
        0.6f + 0.4f * cosf(wavePhase * 0.6f + static_cast<float>(cache[i].darkEnergy))
    );
    baseColor = glm::clamp(baseColor, 0.0f, 1.0f); // Ensure valid RGB

    // Subtle randomness for organic feel (seed with wavePhase)
    static std::mt19937 rng(static_cast<unsigned>(wavePhase * 1000.0f));
    std::uniform_real_distribution<float> noiseDist(-0.05f, 0.05f);
    baseColor += glm::vec3(noiseDist(rng), noiseDist(rng), noiseDist(rng));

    PushConstants pushConstants = {
        model,
        view,
        proj,
        baseColor,
        value,
        1.0f, // Dimension 1
        wavePhase,
        cycleProgress,
        static_cast<float>(cache[i].darkMatter),
        static_cast<float>(cache[i].darkEnergy)
    };
    vkCmdPushConstants(commandBuffer, pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);

    amouranth->setCurrentDimension(1);
    auto pairs = amouranth->getInteractions();
    if (pairs.empty()) {
        std::cerr << "Warning: No interactions for dimension 1\n";
        // Fallback: A subtle "echo" sphere
        glm::mat4 fallbackModel = glm::translate(glm::mat4(1.0f), glm::vec3(linearPos * 0.5f, 0.0f, 0.0f));
        fallbackModel = glm::scale(fallbackModel, glm::vec3(0.1f * zoomFactor * pulse, 0.1f * zoomFactor * pulse, 0.1f * zoomFactor * pulse));
        glm::vec3 fallbackColor = baseColor * 0.5f; // Dimmed version
        PushConstants fallbackPush = {
            fallbackModel, view, proj, fallbackColor, value * 0.3f, 1.0f, wavePhase, cycleProgress, 0.2f, 0.2f
        };
        vkCmdPushConstants(commandBuffer, pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &fallbackPush);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
    } else {
        // Enhanced interactions: Chain-like orbiting, with velocity-inspired trails (multiple draws)
        for (size_t idx = 0; idx < pairs.size(); ++idx) {
            if (amouranth->getMode() != 1) continue;

            const auto& pair = pairs[idx];
            float interactionStrength = static_cast<float>(
                amouranth->computeInteraction(pair.vertexIndex, pair.distance) *
                std::exp(-glm::abs(static_cast<float>(amouranth->getAlpha()) * pair.distance)) *
                amouranth->computePermeation(pair.vertexIndex) *
                glm::max(0.0f, static_cast<float>(pair.strength))
            );
            interactionStrength = glm::clamp(interactionStrength, 0.01f, 1.5f); // Amp up for visibility

            // Position: Orbit around main pos, with radial distance tied to strength
            float orbitalRadius = static_cast<float>(pair.distance) * 1.2f * (1.0f + interactionStrength * 0.8f);
            float orbitalSpeed = 1.5f + static_cast<float>(pair.vertexIndex) * 0.3f;
            float angle = wavePhase * orbitalSpeed + pair.vertexIndex * glm::pi<float>() / 3.0f + cycleProgress * glm::two_pi<float>();
            glm::vec3 orbitalOffset = glm::vec3(
                orbitalRadius * cosf(angle),
                orbitalRadius * sinf(angle) * 0.7f, // Elliptical for interest
                static_cast<float>(pair.strength) * sinf(wavePhase * 0.7f) // Z-bob
            );
            glm::vec3 offsetPos = pos + orbitalOffset;

            // Scale with trail effect: Smaller for "older" interactions
            float trailFade = 1.0f - static_cast<float>(idx) / static_cast<float>(pairs.size());
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), offsetPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(
                0.2f * zoomFactor * interactionStrength * trailFade,
                0.2f * zoomFactor * interactionStrength * trailFade,
                0.2f * zoomFactor * interactionStrength * trailFade
            ));

            // Reactive color: Gradient from main to interaction-specific
            float colorLerp = interactionStrength * trailFade;
            glm::vec3 interactionColor = glm::mix(baseColor, glm::vec3(
                0.2f + 0.3f * sinf(angle * 1.5f),
                0.8f - 0.4f * cosf(angle),
                0.4f + 0.5f * sinf(angle * 0.9f + wavePhase)
            ), colorLerp);

            // Add noise for sparkle
            static std::mt19937 interactionRng(static_cast<unsigned>(wavePhase * 1000.0f + pair.vertexIndex));
            std::uniform_real_distribution<float> sparkleDist(0.9f, 1.1f);
            interactionColor *= sparkleDist(interactionRng);

            PushConstants interactionPush = {
                interactionModel,
                view,
                proj,
                interactionColor,
                interactionStrength * (0.6f + 0.4f * cosf(wavePhase * 1.1f + pair.distance)),
                1.0f,
                wavePhase,
                cycleProgress,
                static_cast<float>(pair.strength),
                static_cast<float>(amouranth->computeDarkEnergy(pair.distance))
            };
            vkCmdPushConstants(commandBuffer, pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &interactionPush);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);

            // Bonus: Draw a faint "trail" sphere behind for motion blur effect (every other interaction)
            if (idx % 2 == 0) {
                float trailAlpha = trailFade * 0.4f;
                glm::vec3 trailPos = pos + orbitalOffset * 0.7f; // Lagged position
                glm::mat4 trailModel = glm::translate(glm::mat4(1.0f), trailPos);
                trailModel = glm::scale(trailModel, glm::vec3(0.1f * zoomFactor * trailAlpha, 0.1f * zoomFactor * trailAlpha, 0.1f * zoomFactor * trailAlpha));
                glm::vec3 trailColor = interactionColor * 0.3f;
                PushConstants trailPush = {
                    trailModel, view, proj, trailColor, interactionStrength * trailAlpha, 1.0f, wavePhase, cycleProgress, 0.1f, 0.1f
                };
                vkCmdPushConstants(commandBuffer, pipelineLayout,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(PushConstants), &trailPush);
                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
            }
        }
    }
}