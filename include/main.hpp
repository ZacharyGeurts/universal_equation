#ifndef MAIN_HPP
#define MAIN_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <iostream>
#include <map>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vulkan/vulkan.h>
#include "SDL3_init.hpp"
#include "Vulkan_init.hpp"
#include "universal_equation.hpp"

// Structure to hold dimension data, including dark matter and dark energy contributions.
struct DimensionData {
    int dimension;                // Dimension number
    double observable;            // Observable energy fluctuation
    double potential;             // Potential energy fluctuation
    double darkMatter;            // Dark matter contribution
    double darkEnergy;            // Dark energy contribution
};

class DimensionalNavigator {
public:
    DimensionalNavigator(const char* title = "Dimensional Navigator",
                        int width = 1280, int height = 720,
                        const char* fontPath = "arial.ttf", int fontSize = 24)
        : window_(nullptr), font_(nullptr), vulkanInstance_(VK_NULL_HANDLE), vulkanDevice_(VK_NULL_HANDLE),
          physicalDevice_(VK_NULL_HANDLE), surface_(VK_NULL_HANDLE), swapchain_(VK_NULL_HANDLE),
          mode_(1), wavePhase_(0.0f), waveSpeed_(0.1f), width_(width), height_(height) {
        try {
            SDL3Initializer::initializeSDL(window_, vulkanInstance_, surface_, font_, title, width, height, fontPath, fontSize);
            initializeSphereGeometry();
            initializeQuadGeometry();
            initializeVulkan();
            initializeCalculator();
        } catch (const std::exception& e) {
            std::cerr << "Initialization failed: " << e.what() << "\n";
            cleanup();
            throw;
        }
    }

    ~DimensionalNavigator() {
        cleanup();
    }

    void run() {
        bool running = true;
        SDL_Event event;
        while (running) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
                    try {
                        recreateSwapchain();
                    } catch (const std::exception& e) {
                        std::cerr << "Swapchain recreation failed: " << e.what() << "\n";
                        running = false;
                    }
                }
                handleInput(event);
            }
            try {
                render();
            } catch (const std::exception& e) {
                std::cerr << "Render failed: " << e.what() << "\n";
                running = false;
            }
            SDL_Delay(16);
        }
    }

    void adjustInfluence(double delta) {
        ue_.setInfluence(std::max(0.0, ue_.getInfluence() + delta));
        updateCache();
    }

    void adjustDarkMatter(double delta) {
        ue_.setDarkMatterStrength(std::max(0.0, ue_.getDarkMatterStrength() + delta));
        updateCache();
    }

    void adjustDarkEnergy(double delta) {
        ue_.setDarkEnergyStrength(std::max(0.0, ue_.getDarkEnergyStrength() + delta));
        updateCache();
    }

private:
    SDL_Window* window_;
    TTF_Font* font_;
    VkInstance vulkanInstance_;
    VkDevice vulkanDevice_;
    VkPhysicalDevice physicalDevice_;
    VkQueue graphicsQueue_;
    VkQueue presentQueue_;
    VkSurfaceKHR surface_;
    VkSwapchainKHR swapchain_;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> swapchainFramebuffers_;
    VkPipeline pipeline_;
    VkPipelineLayout pipelineLayout_;
    VkRenderPass renderPass_;
    VkCommandPool commandPool_;
    std::vector<VkCommandBuffer> commandBuffers_;
    VkSemaphore imageAvailableSemaphore_;
    VkSemaphore renderFinishedSemaphore_;
    VkFence inFlightFence_;
    VkBuffer vertexBuffer_;
    VkDeviceMemory vertexBufferMemory_;
    VkBuffer indexBuffer_;
    VkDeviceMemory indexBufferMemory_;
    VkBuffer quadVertexBuffer_;
    VkDeviceMemory quadVertexBufferMemory_;
    VkBuffer quadIndexBuffer_;
    VkDeviceMemory quadIndexBufferMemory_;
    UniversalEquation ue_;
    std::vector<DimensionData> cache_;
    int mode_;
    float wavePhase_;
    const float waveSpeed_;
    int width_;
    int height_;
    std::vector<glm::vec3> sphereVertices_;
    std::vector<uint32_t> sphereIndices_;
    std::vector<glm::vec3> quadVertices_;
    std::vector<uint32_t> quadIndices_;
    uint32_t graphicsFamily_ = UINT32_MAX;
    uint32_t presentFamily_ = UINT32_MAX;
    static constexpr int kMaxRenderedDimensions = 4;

    struct PushConstants {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
        float value;
        float dimension;
        float wavePhase;
        float cycleProgress;
        float darkMatter;
        float darkEnergy;
    };

    void initializeVulkan() {
        VulkanInitializer::initializeVulkan(vulkanInstance_, physicalDevice_, vulkanDevice_, surface_,
                                            graphicsQueue_, presentQueue_, graphicsFamily_, presentFamily_,
                                            swapchain_, swapchainImages_, swapchainImageViews_, renderPass_,
                                            pipeline_, pipelineLayout_, swapchainFramebuffers_, commandPool_,
                                            commandBuffers_, imageAvailableSemaphore_, renderFinishedSemaphore_,
                                            inFlightFence_, vertexBuffer_, vertexBufferMemory_, indexBuffer_,
                                            indexBufferMemory_, sphereVertices_, sphereIndices_, width_, height_);
        VulkanInitializer::initializeQuadBuffers(vulkanDevice_, physicalDevice_, commandPool_, graphicsQueue_,
                                                quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_,
                                                quadIndexBufferMemory_, quadVertices_, quadIndices_);
    }

    void recreateSwapchain() {
        if (vulkanDevice_) vkDeviceWaitIdle(vulkanDevice_);
        VulkanInitializer::cleanupVulkan(vulkanInstance_, vulkanDevice_, surface_, swapchain_, swapchainImageViews_,
                                         swapchainFramebuffers_, pipeline_, pipelineLayout_, renderPass_,
                                         commandPool_, commandBuffers_, imageAvailableSemaphore_, renderFinishedSemaphore_,
                                         inFlightFence_, vertexBuffer_, vertexBufferMemory_, indexBuffer_, indexBufferMemory_);
        VulkanInitializer::cleanupQuadBuffers(vulkanDevice_, quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_, quadIndexBufferMemory_);
        vulkanDevice_ = VK_NULL_HANDLE;
        swapchain_ = VK_NULL_HANDLE;
        swapchainImages_.clear();
        swapchainImageViews_.clear();
        swapchainFramebuffers_.clear();
        pipeline_ = VK_NULL_HANDLE;
        pipelineLayout_ = VK_NULL_HANDLE;
        renderPass_ = VK_NULL_HANDLE;
        commandPool_ = VK_NULL_HANDLE;
        commandBuffers_.clear();
        imageAvailableSemaphore_ = VK_NULL_HANDLE;
        renderFinishedSemaphore_ = VK_NULL_HANDLE;
        inFlightFence_ = VK_NULL_HANDLE;
        vertexBuffer_ = VK_NULL_HANDLE;
        vertexBufferMemory_ = VK_NULL_HANDLE;
        indexBuffer_ = VK_NULL_HANDLE;
        indexBufferMemory_ = VK_NULL_HANDLE;
        quadVertexBuffer_ = VK_NULL_HANDLE;
        quadVertexBufferMemory_ = VK_NULL_HANDLE;
        quadIndexBuffer_ = VK_NULL_HANDLE;
        quadIndexBufferMemory_ = VK_NULL_HANDLE;
        int newWidth, newHeight;
        SDL_GetWindowSize(window_, &newWidth, &newHeight);
        width_ = newWidth;
        height_ = newHeight;
        initializeVulkan();
    }

    void cleanup() {
        VulkanInitializer::cleanupVulkan(vulkanInstance_, vulkanDevice_, surface_, swapchain_, swapchainImageViews_,
                                         swapchainFramebuffers_, pipeline_, pipelineLayout_, renderPass_,
                                         commandPool_, commandBuffers_, imageAvailableSemaphore_, renderFinishedSemaphore_,
                                         inFlightFence_, vertexBuffer_, vertexBufferMemory_, indexBuffer_, indexBufferMemory_);
        VulkanInitializer::cleanupQuadBuffers(vulkanDevice_, quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_, quadIndexBufferMemory_);
        SDL3Initializer::cleanupSDL(window_, vulkanInstance_, surface_, font_);
        window_ = nullptr;
        font_ = nullptr;
        vulkanInstance_ = VK_NULL_HANDLE;
        surface_ = VK_NULL_HANDLE;
    }

    void initializeSphereGeometry() {
        const int stacks = 16;
        const int slices = 16;
        sphereVertices_.clear();
        sphereIndices_.clear();

        for (int i = 0; i <= stacks; ++i) {
            float phi = i * M_PI / stacks;
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);
            for (int j = 0; j <= slices; ++j) {
                float theta = j * 2.0f * M_PI / slices;
                float sinTheta = sin(theta);
                float cosTheta = cos(theta);
                sphereVertices_.push_back(glm::vec3(sinPhi * cosTheta, cosPhi, sinPhi * sinTheta));
            }
        }

        for (int i = 0; i < stacks; ++i) {
            for (int j = 0; j < slices; ++j) {
                uint32_t v0 = i * (slices + 1) + j;
                uint32_t v1 = v0 + 1;
                uint32_t v2 = (i + 1) * (slices + 1) + j;
                uint32_t v3 = v2 + 1;
                sphereIndices_.insert(sphereIndices_.end(), {v0, v1, v2, v2, v1, v3});
            }
        }

        std::cerr << "Initialized sphere: " << sphereVertices_.size() << " vertices, " << sphereIndices_.size() << " indices\n";
    }

    void initializeQuadGeometry() {
        quadVertices_ = {
            {-1.0f, -1.0f, 0.0f}, {1.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f}
        };
        quadIndices_ = {0, 1, 2, 2, 3, 0};
        std::cerr << "Initialized quad: " << quadVertices_.size() << " vertices, " << quadIndices_.size() << " indices\n";
    }

    void initializeCalculator() {
        ue_ = UniversalEquation(9, 1, 1.0, 0.5, 0.5, 0.5, 1.5, 2.0, 0.27, 0.68, 5.0, 0.2, false);
        updateCache();
    }

    void updateCache() {
        cache_.clear();
        cache_.reserve(kMaxRenderedDimensions);
        for (int d = 1; d <= kMaxRenderedDimensions; ++d) {
            ue_.setCurrentDimension(d);
            auto result = ue_.compute();
            cache_.push_back({d, result.observable, result.potential, result.darkMatter, result.darkEnergy});
        }
    }

    void handleInput(const SDL_Event& event) {
        if (event.type == SDL_EVENT_KEY_DOWN) {
            switch (event.key.key) {
                case SDLK_UP:
                    adjustInfluence(0.1);
                    break;
                case SDLK_DOWN:
                    adjustInfluence(-0.1);
                    break;
                case SDLK_LEFT:
                    adjustDarkMatter(-0.05);
                    break;
                case SDLK_RIGHT:
                    adjustDarkMatter(0.05);
                    break;
                case SDLK_PAGEUP:
                    adjustDarkEnergy(0.05);
                    break;
                case SDLK_PAGEDOWN:
                    adjustDarkEnergy(-0.05);
                    break;
                case SDLK_1:
                    mode_ = 1;
                    break;
                case SDLK_2:
                    mode_ = 2;
                    break;
                case SDLK_3:
                    mode_ = 3;
                    break;
                case SDLK_4:
                    mode_ = 4;
                    break;
            }
        }
    }

    void render() {
        vkWaitForFences(vulkanDevice_, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
        vkResetFences(vulkanDevice_, 1, &inFlightFence_);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vulkanDevice_, swapchain_, UINT64_MAX, imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            return;
        }
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to acquire swapchain image: " + std::to_string(result));
        }

        vkResetCommandBuffer(commandBuffers_[imageIndex], 0);

        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr };
        if (vkBeginCommandBuffer(commandBuffers_[imageIndex], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin command buffer");
        }

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkRenderPassBeginInfo renderPassInfo = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, renderPass_, swapchainFramebuffers_[imageIndex], {{0, 0}, {(uint32_t)width_, (uint32_t)height_}}, 1, &clearColor };
        vkCmdBeginRenderPass(commandBuffers_[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffers_[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

        updateCache();
        if (mode_ == 1) {
            renderMode1(imageIndex);
        } else if (mode_ == 2) {
            renderMode2(imageIndex);
        } else if (mode_ == 3) {
            renderMode3(imageIndex);
        } else if (mode_ == 4) {
            renderMode4(imageIndex);
        }

        vkCmdEndRenderPass(commandBuffers_[imageIndex]);
        if (vkEndCommandBuffer(commandBuffers_[imageIndex]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to end command buffer");
        }

        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSubmitInfo submitInfo = { VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1, &imageAvailableSemaphore_, waitStages, 1, &commandBuffers_[imageIndex], 1, &renderFinishedSemaphore_ };
        if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFence_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit queue");
        }

        VkPresentInfoKHR presentInfo = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1, &renderFinishedSemaphore_, 1, &swapchain_, &imageIndex, nullptr };
        result = vkQueuePresentKHR(presentQueue_, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            return;
        }
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present queue: " + std::to_string(result));
        }

        wavePhase_ += waveSpeed_;
    }

    void renderMode1(uint32_t imageIndex) {
        VkBuffer vertexBuffers[] = {vertexBuffer_};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffers_[imageIndex], indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width_ / height_, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        constexpr float baseRadius = 0.5f;
        float cycleProgress = std::fmod(wavePhase_ / (2.0f * kMaxRenderedDimensions), 1.0f);
        for (size_t i = 0; i < kMaxRenderedDimensions; ++i) {
            float emphasis = (i == 0) ? 2.0f : 0.5f;
            float radius = baseRadius * emphasis * (1.0f + static_cast<float>(cache_[i].observable) * 0.2f) *
                           (1.0f + 0.1f * sin(wavePhase_ + i) * (1.0f + static_cast<float>(cache_[i].darkMatter) * 0.5f));
            glm::mat4 model = glm::rotate(glm::mat4(1.0f), wavePhase_ * 0.2f, glm::vec3(0.0f, 1.0f, 0.0f));
            if (i == 0) {
                model = glm::scale(model, glm::vec3(radius));
            } else if (i == 1) {
                model = glm::translate(model, glm::vec3(0.0f, 1.5f * (1.0f + static_cast<float>(cache_[i].darkEnergy) * 0.2f), -i * 1.0f));
                model = glm::scale(model, glm::vec3(radius));
            } else {
                model = glm::translate(model, glm::vec3(0.0f, 0.0f, -i * 1.0f * (1.0f + static_cast<float>(cache_[i].darkEnergy) * 0.2f)));
                model = glm::scale(model, glm::vec3(radius));
            }

            float dimValue = static_cast<float>(i + 1);
            PushConstants pushConstants = {model, view, proj, static_cast<float>(cache_[i].observable), dimValue, wavePhase_, cycleProgress,
                                          static_cast<float>(cache_[i].darkMatter), static_cast<float>(cache_[i].darkEnergy)};
            vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_, 
                              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                              0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);
        }
    }

    void renderMode2(uint32_t imageIndex) {
        VkBuffer vertexBuffers[] = {vertexBuffer_};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffers_[imageIndex], indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width_ / height_, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        constexpr float baseRadius = 0.5f;
        float cycleProgress = std::fmod(wavePhase_ / (2.0f * kMaxRenderedDimensions), 1.0f);
        for (size_t i = 0; i < kMaxRenderedDimensions; ++i) {
            float emphasis = (i == 1) ? 2.0f : 0.5f;
            float radius = baseRadius * emphasis * (1.0f + static_cast<float>(cache_[i].observable) * 0.2f) *
                           (1.0f + 0.1f * sin(wavePhase_ + i) * (1.0f + static_cast<float>(cache_[i].darkMatter) * 0.5f));
            glm::mat4 model = glm::mat4(1.0f);
            float angle = wavePhase_ + (i + 1) * 2.0f * glm::pi<float>() / kMaxRenderedDimensions;
            float spacing = 1.5f * (1.0f + static_cast<float>(cache_[i].darkEnergy) * 0.5f);
            float x = cos(angle) * spacing;
            float y = sin(angle) * spacing;
            float z = -i * 0.5f;
            if (i == 1) {
                model = glm::translate(model, glm::vec3(x * 1.5f, y * 1.5f, z));
                model = glm::scale(model, glm::vec3(radius));
            } else if (i == 0) {
                model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
                model = glm::scale(model, glm::vec3(radius));
            } else {
                model = glm::translate(model, glm::vec3(x, y, z));
                model = glm::scale(model, glm::vec3(radius));
            }

            float dimValue = static_cast<float>(i + 1);
            PushConstants pushConstants = {model, view, proj, static_cast<float>(cache_[i].observable), dimValue, wavePhase_, cycleProgress,
                                          static_cast<float>(cache_[i].darkMatter), static_cast<float>(cache_[i].darkEnergy)};
            vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_, 
                              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                              0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);
        }
    }

    void renderMode3(uint32_t imageIndex) {
        VkBuffer vertexBuffers[] = {vertexBuffer_};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffers_[imageIndex], indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

        glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width_ / height_, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        constexpr float baseRadius = 0.5f;
        float cycleProgress = std::fmod(wavePhase_ / (2.0f * kMaxRenderedDimensions), 1.0f);
        for (size_t i = 0; i < kMaxRenderedDimensions; ++i) {
            float emphasis = (i == 2) ? 2.0f : 0.5f;
            float radius = baseRadius * emphasis * (1.0f + static_cast<float>(cache_[i].observable) * 0.2f) *
                           (1.0f + 0.1f * sin(wavePhase_ + i) * (1.0f + static_cast<float>(cache_[i].darkMatter) * 0.5f));
            glm::mat4 model = glm::mat4(1.0f);
            if (i == 2) {
                model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
                model = glm::scale(model, glm::vec3(radius));
            } else {
                float offsetX = (i == 0) ? -1.5f : (i == 1) ? 0.0f : 1.5f;
                model = glm::translate(model, glm::vec3(offsetX * (1.0f + static_cast<float>(cache_[i].darkEnergy) * 0.2f), 0.0f, -i * 1.0f));
                model = glm::scale(model, glm::vec3(radius));
            }

            float dimValue = static_cast<float>(i + 1);
            PushConstants pushConstants = {model, view, proj, static_cast<float>(cache_[i].observable), dimValue, wavePhase_, cycleProgress,
                                          static_cast<float>(cache_[i].darkMatter), static_cast<float>(cache_[i].darkEnergy)};
            vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_, 
                              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                              0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);

            if (i == 2) {
                ue_.setCurrentDimension(3);
                auto pairs = ue_.getInteractions();
                for (const auto& pair : pairs) {
                    if (pair.dimension > kMaxRenderedDimensions) continue;
                    float interactionStrength = static_cast<float>(
                        computeInteraction(pair.dimension, pair.distance) *
                        std::exp(-ue_.getAlpha() * pair.distance) *
                        computePermeation(pair.dimension) *
                        pair.darkMatterDensity);
                    float interactionRadius = baseRadius * 0.3f * interactionStrength * (1.0f + 0.1f * sin(wavePhase_));
                    if (interactionRadius > 0.01f) {
                        glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), 
                                                                   glm::vec3(0.5f * (pair.dimension - 3) * (1.0f + static_cast<float>(pair.distance) * 0.5f), 
                                                                             0.0f, 0.0f));
                        interactionModel = glm::scale(interactionModel, glm::vec3(interactionRadius));
                        pushConstants = {interactionModel, view, proj, interactionStrength, static_cast<float>(pair.dimension), wavePhase_, cycleProgress,
                                        static_cast<float>(pair.darkMatterDensity), static_cast<float>(computeDarkEnergy(pair.distance))};
                        vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_, 
                                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                          0, sizeof(PushConstants), &pushConstants);
                        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);
                    }
                }
            }
        }
    }

void renderMode4(uint32_t imageIndex) {
    VkBuffer vertexBuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffers_[imageIndex], indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width_ / height_, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    constexpr float baseRadius = 0.5f;
    float cycleProgress = std::fmod(wavePhase_ / (2.0f * kMaxRenderedDimensions), 1.0f);

    for (size_t i = 0; i < kMaxRenderedDimensions; ++i) {
        // Define components of the equation
        float alpha = (i == 3) ? 2.0f : 0.5f; // α: emphasis factor
        float omega = 0.2f; // ω: sinusoidal modulation factor
        float observableRadius = baseRadius * (1.0f + static_cast<float>(cache_[i].observable) * 0.2f); // K
        float potentialRadius = baseRadius * (1.0f + static_cast<float>(cache_[i].potential) * 0.2f); // ∇Φ component
        float timeModulation = sin(wavePhase_ + i) * (1.0f + static_cast<float>(cache_[i].darkMatter) * 0.5f); // T

        // Geometry scaling (G)
        float geometryScale = 0.5f * (observableRadius + potentialRadius); // Base G
        float scaledGeometry = geometryScale * (alpha + omega * timeModulation); // α⋅G + ω⋅G

        // Location (L) and Energy (E)
        glm::mat4 model = glm::mat4(1.0f);
        if (i == 3) {
            model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // L: no offset for dimension 3
        } else {
            float offsetX = (i == 0) ? -1.5f : (i == 1) ? 0.0f : 1.5f;
            float energyOffset = offsetX * (1.0f + static_cast<float>(cache_[i].darkEnergy) * 0.2f); // E
            model = glm::translate(model, glm::vec3(energyOffset, 0.0f, -i * 1.0f)); // L + E
        }

        // Combine transformations: T = (L + E + K + G) + α⋅G + ω⋅G + ∇Φ
        float finalScale = scaledGeometry + potentialRadius * 0.1f; // Add ∇Φ influence
        model = glm::scale(model, glm::vec3(finalScale));

        // Push constants
        float fluctuationRatio = cache_[i].potential > 0 ? static_cast<float>(cache_[i].observable / cache_[i].potential) : 1.0f;
        float dimValue = static_cast<float>(i + 1);
        PushConstants pushConstants = {model, view, proj, fluctuationRatio, dimValue, wavePhase_, cycleProgress,
                                      static_cast<float>(cache_[i].darkMatter), static_cast<float>(cache_[i].darkEnergy)};
        vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_, 
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);
    }

    // 1D case (similar adjustments can be applied)
    float radius1D = baseRadius * (1.0f + static_cast<float>(ue_.getInfluence() * ue_.getOneDPermeation()) * 0.2f);
    float timeModulation1D = 0.3f * sin(wavePhase_) * (1.0f + static_cast<float>(ue_.getDarkMatterStrength()) * 0.5f);
    float finalScale1D = radius1D * (1.0f + timeModulation1D); // G + ω⋅G
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.5f, 0.0f)); // L
    model = glm::scale(model, glm::vec3(finalScale1D * 1.5f));

    PushConstants pushConstants = {model, view, proj, 1.0f, 1.0f, wavePhase_, cycleProgress,
                                  static_cast<float>(ue_.getDarkMatterStrength()), static_cast<float>(ue_.getDarkEnergyStrength())};
    vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_, 
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                      0, sizeof(PushConstants), &pushConstants);
    vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);
}

private:
    // Moved from UniversalEquation to allow access in renderMode3
    double computeInteraction(int dimension, double distance) const {
        double denom = std::max(1e-15, std::pow(static_cast<double>(ue_.getCurrentDimension()), dimension));
        double modifier = (ue_.getCurrentDimension() > 3 && dimension > 3) ? ue_.getWeak() : 1.0;
        if (ue_.getCurrentDimension() == 3 && (dimension == 2 || dimension == 4)) {
            modifier *= ue_.getThreeDInfluence();
        }
        double result = ue_.getInfluence() * (distance / denom) * modifier;
        if (ue_.getDebug()) {
            std::cout << "Interaction(D=" << dimension << ", dist=" << distance << "): " << result << "\n";
        }
        return result;
    }

    double computePermeation(int dimension) const {
        if (dimension == 1 || ue_.getCurrentDimension() == 1) return ue_.getOneDPermeation();
        if (ue_.getCurrentDimension() == 2 && dimension > 2) return ue_.getTwoD();
        if (ue_.getCurrentDimension() == 3 && (dimension == 2 || dimension == 4)) return ue_.getThreeDInfluence();
        return 1.0;
    }

    double computeDarkEnergy(double distance) const {
        double result = ue_.getDarkEnergyStrength() * std::exp(distance * (ue_.getMaxDimensions() > 0 ? 1.0 / ue_.getMaxDimensions() : 1e-15));
        if (ue_.getDebug()) {
            std::cout << "DarkEnergy(dist=" << distance << "): " << result << "\n";
        }
        return result;
    }
};

#endif // MAIN_HPP