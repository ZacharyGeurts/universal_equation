#include "core.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <vulkan/vulkan.h>
#include <iostream>
#include <cmath>
#include <random> // For subtle randomness

static constexpr int kMaxRenderedDimensions = 9;

void renderMode2(AMOURANTH* amouranth, uint32_t imageIndex, VkBuffer vertexBuffer, VkCommandBuffer commandBuffer, VkBuffer indexBuffer, float zoomLevel, int width, int height, float wavePhase, const std::vector<DimensionData>& cache, VkPipelineLayout pipelineLayout) {
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

    // Dynamic camera for 2D: Tilted view for planar emphasis, with gentle pan
    glm::vec3 camPos(0.0f, 4.0f * zoomFactor, 5.0f * zoomFactor);
    if (amouranth->isUserCamActive()) {
        camPos = amouranth->getUserCamPos();
    }
    glm::vec3 camTarget(0.0f, 0.0f, 0.0f);
    glm::vec3 camUp(0.0f, 1.0f, 0.0f);
    // Pan in yz plane for 2D sweep
    float panAngle = wavePhase * 0.15f;
    camPos.y = 3.0f * zoomFactor * cosf(panAngle);
    camPos.z = 4.0f * zoomFactor * sinf(panAngle) + 2.0f * zoomFactor;
    glm::mat4 view = glm::lookAt(camPos, camTarget, camUp);

    float cycleProgress = std::fmod(wavePhase / (2.0f * kMaxRenderedDimensions), 1.0f);

    if (cache.size() < kMaxRenderedDimensions) {
        std::cerr << "Warning: Cache size " << cache.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    size_t i = 1; // Dimension 2
    if (cache[i].dimension != 2) {
        std::cerr << "Warning: Invalid cache for dimension 2\n";
        return;
    }

    // Enhanced oscillation: Multi-harmonic for 2D wave-like undulation
    float oscX = 1.0f + 0.4f * sinf(wavePhase * 1.2f + static_cast<float>(cache[i].darkEnergy) * 1.5f);
    float oscY = 1.0f + 0.4f * cosf(wavePhase * 1.0f + static_cast<float>(cache[i].potential) * 2.5f);
    float pulse = 0.6f + 0.4f * sinf(wavePhase * 1.8f + cycleProgress * glm::two_pi<float>());
    float value = static_cast<float>(cache[i].observable * oscX * oscY * pulse);
    value = glm::clamp(value, 0.01f, 2.5f); // Broader dynamic range

    // 2D emphasis: Circular motion in xy-plane, with radial pulsing
    float angularSpeed = 0.9f + static_cast<float>(cache[i].darkMatter) * 0.5f;
    float angle = wavePhase * angularSpeed + i * glm::two_pi<float>() / kMaxRenderedDimensions;
    float scaleFactor = 1.0f + static_cast<float>(cache[i].observable) * 0.6f * pulse;
    float radius = 2.0f * scaleFactor * (1.0f + 0.2f * sinf(wavePhase * 0.7f));
    glm::vec3 pos = glm::vec3(
        radius * cosf(angle + cycleProgress * glm::two_pi<float>()),
        radius * sinf(angle + cycleProgress * glm::two_pi<float>()),
        0.5f * value * sinf(wavePhase * 0.8f) // Subtle z-wave for depth
    );
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, pos);
    // Asymmetric scaling: Wider in xy for planar feel
    model = glm::scale(model, glm::vec3(0.5f * zoomFactor * oscX * scaleFactor,
                                        0.5f * zoomFactor * oscY * scaleFactor,
                                        0.3f * zoomFactor * pulse * scaleFactor));

    // Vibrant, reactive colors: 2D-inspired gradients, cycling hues
    float hueCycle = wavePhase * 0.5f + static_cast<float>(cache[i].darkMatter) * glm::pi<float>();
    glm::vec3 baseColor = glm::vec3(
        0.4f + 0.6f * sinf(hueCycle),
        0.5f + 0.5f * cosf(hueCycle * 1.2f + cycleProgress * glm::pi<float>()),
        0.7f + 0.3f * sinf(hueCycle * 0.8f + static_cast<float>(cache[i].darkEnergy) * 2.0f)
    );
    baseColor = glm::clamp(baseColor, 0.0f, 1.0f);

    // Organic noise for texture
    static std::mt19937 rng(static_cast<unsigned>(wavePhase * 1000.0f + i));
    std::uniform_real_distribution<float> noiseDist(-0.08f, 0.08f);
    baseColor += glm::vec3(noiseDist(rng), noiseDist(rng), noiseDist(rng));

    PushConstants pushConstants = {
        model,
        view,
        proj,
        baseColor,
        value,
        2.0f, // Dimension 2
        wavePhase,
        cycleProgress,
        static_cast<float>(cache[i].darkMatter),
        static_cast<float>(cache[i].darkEnergy)
    };
    vkCmdPushConstants(commandBuffer, pipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);

    amouranth->setCurrentDimension(2);
    auto pairs = amouranth->getInteractions();
    if (pairs.empty()) {
        std::cerr << "Warning: No interactions for dimension 2\n";
        // Fallback: Pulsing ring of echoes
        float fallbackRadius = radius * 0.6f;
        for (int echo = 0; echo < 3; ++echo) {
            float echoAngle = angle + echo * glm::two_pi<float>() / 3.0f;
            glm::vec3 echoPos = glm::vec3(
                fallbackRadius * cosf(echoAngle),
                fallbackRadius * sinf(echoAngle),
                0.0f
            );
            glm::mat4 fallbackModel = glm::translate(glm::mat4(1.0f), echoPos);
            fallbackModel = glm::scale(fallbackModel, glm::vec3(0.15f * zoomFactor * pulse * (1.0f - echo * 0.3f),
                                                                0.15f * zoomFactor * pulse * (1.0f - echo * 0.3f),
                                                                0.15f * zoomFactor * pulse * (1.0f - echo * 0.3f)));
            glm::vec3 fallbackColor = baseColor * (0.4f + 0.3f * (2 - echo));
            PushConstants fallbackPush = {
                fallbackModel, view, proj, fallbackColor, value * 0.4f * (1.0f - echo * 0.2f), 2.0f, wavePhase, cycleProgress, 0.15f, 0.15f
            };
            vkCmdPushConstants(commandBuffer, pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &fallbackPush);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
        }
    } else {
        // Enhanced interactions: Planar orbits with linking "edges" simulated via offset spheres
        for (size_t idx = 0; idx < pairs.size(); ++idx) {
            if (amouranth->getMode() != 2) continue;

            const auto& pair = pairs[idx];
            float interactionStrength = static_cast<float>(
                amouranth->computeInteraction(pair.vertexIndex, pair.distance) *
                std::exp(-glm::abs(static_cast<float>(amouranth->getAlpha()) * pair.distance)) *
                amouranth->computePermeation(pair.vertexIndex) *
                glm::max(0.0f, static_cast<float>(pair.strength))
            );
            interactionStrength = glm::clamp(interactionStrength, 0.01f, 1.8f); // Boost for 2D visibility

            // Position: Co-orbital in plane, with angular separation
            float orbitalRadius = static_cast<float>(pair.distance) * 1.5f * (1.0f + interactionStrength * 1.0f);
            float orbitalSpeed = 1.2f + static_cast<float>(pair.vertexIndex) * 0.4f;
            float sepAngle = pair.vertexIndex * glm::pi<float>() / 4.0f;
            float angle = wavePhase * orbitalSpeed + sepAngle + cycleProgress * glm::two_pi<float>();
            glm::vec3 orbitalOffset = glm::vec3(
                orbitalRadius * cosf(angle),
                orbitalRadius * sinf(angle),
                static_cast<float>(pair.strength) * 0.8f * cosf(wavePhase * 0.6f + sepAngle) // Z-tilt
            );
            glm::vec3 offsetPos = pos + orbitalOffset;

            // Scale with connection fade: Larger for stronger links
            float linkFade = 1.0f - static_cast<float>(idx) / static_cast<float>(pairs.size() * 0.7f);
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), offsetPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(
                0.25f * zoomFactor * interactionStrength * linkFade * oscX,
                0.25f * zoomFactor * interactionStrength * linkFade * oscY,
                0.2f * zoomFactor * interactionStrength * linkFade
            ));

            // Reactive color: Planar palette, lerping to complementary hues
            float colorLerp = interactionStrength * linkFade * 0.7f;
            glm::vec3 interactionColor = glm::mix(baseColor, glm::vec3(
                0.8f + 0.2f * sinf(angle * 1.3f),
                0.3f + 0.4f * cosf(angle * 1.1f + wavePhase * 0.5f),
                0.1f + 0.6f * sinf(angle * 0.7f)
            ), colorLerp);

            // Sparkle noise
            static std::mt19937 interactionRng(static_cast<unsigned>(wavePhase * 1000.0f + pair.vertexIndex * 10 + i));
            std::uniform_real_distribution<float> sparkleDist(0.85f, 1.15f);
            interactionColor *= sparkleDist(interactionRng);

            PushConstants interactionPush = {
                interactionModel,
                view,
                proj,
                interactionColor,
                interactionStrength * (0.7f + 0.3f * sinf(wavePhase * 1.3f + pair.distance)),
                2.0f,
                wavePhase,
                cycleProgress,
                static_cast<float>(pair.strength),
                static_cast<float>(amouranth->computeDarkEnergy(pair.distance))
            };
            vkCmdPushConstants(commandBuffer, pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                               0, sizeof(PushConstants), &interactionPush);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);

            // Bonus: "Link" spheres midway to main pos for edge simulation (every third interaction)
            if (idx % 3 == 0) {
                glm::vec3 midPos = (pos + offsetPos) * 0.5f;
                float linkAlpha = linkFade * 0.3f * interactionStrength;
                glm::mat4 linkModel = glm::translate(glm::mat4(1.0f), midPos);
                linkModel = glm::scale(linkModel, glm::vec3(0.08f * zoomFactor * linkAlpha, 0.08f * zoomFactor * linkAlpha, 0.08f * zoomFactor * linkAlpha));
                glm::vec3 linkColor = glm::mix(baseColor, interactionColor, 0.5f) * 0.6f;
                PushConstants linkPush = {
                    linkModel, view, proj, linkColor, interactionStrength * linkAlpha, 2.0f, wavePhase, cycleProgress, 0.05f, 0.05f
                };
                vkCmdPushConstants(commandBuffer, pipelineLayout,
                                   VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                   0, sizeof(PushConstants), &linkPush);
                vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(amouranth->getSphereIndices().size()), 1, 0, 0, 0);
            }
        }
    }
}