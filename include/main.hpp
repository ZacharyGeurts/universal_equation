#ifndef MAIN_HPP
#define MAIN_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>
#include <iostream>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <vulkan/vulkan.h>
#include "SDL3_init.hpp"
#include "Vulkan_init.hpp"
#include "universal_equation.hpp"

struct DimensionData {
    int dimension;
    double positive;
    double negative;
};

class DimensionalNavigator {
public:
    DimensionalNavigator(const char* title = "Dimensional Navigator",
                        int width = 1280, int height = 720,
                        const char* fontPath = "assets/fonts/arial.ttf", int fontSize = 24)
        : window_(nullptr), font_(nullptr), vulkanInstance_(VK_NULL_HANDLE), vulkanDevice_(VK_NULL_HANDLE),
          physicalDevice_(VK_NULL_HANDLE), surface_(VK_NULL_HANDLE), swapchain_(VK_NULL_HANDLE),
          mode_(1), wavePhase_(0.0f), waveSpeed_(0.1f), width_(width), height_(height) {
        try {
            SDL3Initializer::initializeSDL(window_, vulkanInstance_, surface_, font_, title, width, height, fontPath, fontSize);
            initializeSphereGeometry();
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
    UniversalEquation ue_;
    std::vector<DimensionData> cache_;
    int mode_;
    float wavePhase_;
    const float waveSpeed_;
    int width_;
    int height_;
    std::vector<glm::vec3> sphereVertices_;
    std::vector<uint32_t> sphereIndices_;
    uint32_t graphicsFamily_ = UINT32_MAX;
    uint32_t presentFamily_ = UINT32_MAX;

    void initializeVulkan() {
        VulkanInitializer::initializeVulkan(vulkanInstance_, physicalDevice_, vulkanDevice_, surface_,
                                            graphicsQueue_, presentQueue_, graphicsFamily_, presentFamily_,
                                            swapchain_, swapchainImages_, swapchainImageViews_, renderPass_,
                                            pipeline_, pipelineLayout_, swapchainFramebuffers_, commandPool_,
                                            commandBuffers_, imageAvailableSemaphore_, renderFinishedSemaphore_,
                                            inFlightFence_, vertexBuffer_, vertexBufferMemory_, indexBuffer_,
                                            indexBufferMemory_, sphereVertices_, sphereIndices_, width_, height_);
    }

    void recreateSwapchain() {
        if (vulkanDevice_) vkDeviceWaitIdle(vulkanDevice_);
        VulkanInitializer::cleanupVulkan(VK_NULL_HANDLE, vulkanDevice_, VK_NULL_HANDLE, swapchain_, swapchainImageViews_,
                                         swapchainFramebuffers_, pipeline_, pipelineLayout_, renderPass_,
                                         commandPool_, commandBuffers_, imageAvailableSemaphore_, renderFinishedSemaphore_,
                                         inFlightFence_, vertexBuffer_, vertexBufferMemory_, indexBuffer_, indexBufferMemory_);
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
        SDL3Initializer::cleanupSDL(window_, vulkanInstance_, surface_, font_);
        window_ = nullptr;
        font_ = nullptr;
        vulkanInstance_ = VK_NULL_HANDLE;
        surface_ = VK_NULL_HANDLE;
    }

    void initializeSphereGeometry() {
        const float t = (1.0f + std::sqrt(5.0f)) / 2.0f;
        sphereVertices_ = {
            {-1.0f, t, 0.0f}, {1.0f, t, 0.0f}, {-1.0f, -t, 0.0f}, {1.0f, -t, 0.0f},
            {0.0f, -1.0f, t}, {0.0f, 1.0f, t}, {0.0f, -1.0f, -t}, {0.0f, 1.0f, -t},
            {t, 0.0f, -1.0f}, {t, 0.0f, 1.0f}, {-t, 0.0f, -1.0f}, {-t, 0.0f, 1.0f}
        };
        for (auto& v : sphereVertices_) {
            v = glm::normalize(v);
        }
        sphereIndices_ = {
            0, 11, 5, 0, 5, 1, 0, 1, 7, 0, 7, 10, 0, 10, 11,
            1, 5, 9, 5, 11, 4, 11, 10, 2, 10, 7, 6, 7, 1, 8,
            3, 9, 4, 3, 4, 2, 3, 2, 6, 3, 6, 8, 3, 8, 9,
            4, 9, 5, 2, 4, 11, 6, 2, 10, 8, 6, 7, 9, 8, 1
        };
        std::cerr << "Initialized sphere: " << sphereVertices_.size() << " vertices, " << sphereIndices_.size() << " indices\n";
    }

    void initializeCalculator() {
        ue_ = UniversalEquation(9, 1.0, 0.5, 0.5, 0.5, 2.0, 5.0, 0.2);
        updateCache();
    }

    void updateCache() {
        cache_.clear();
        for (int d = 1; d <= ue_.getMaxDimensions(); ++d) {
            ue_.setCurrentDimension(d);
            auto [pos, neg] = ue_.compute();
            cache_.push_back({d, pos, neg});
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
                case SDLK_1:
                    mode_ = 1;
                    break;
                case SDLK_2:
                    mode_ = 2;
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

        if (mode_ == 1) {
            renderMode1(imageIndex);
        } else {
            renderMode2(imageIndex);
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
    struct PushConstants {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
        float radius;
        float wavePhase;
    };

    // Set current dimension to 2 for 2D rendering
    ue_.setCurrentDimension(2);
    auto [pos, neg] = ue_.compute();
    float radius = 100.0f * (1.0f + static_cast<float>(pos) * 0.1f); // Scale radius based on positive influence

    // Orthographic projection for 2D
    glm::mat4 ortho = glm::ortho(0.0f, (float)width_, (float)height_, 0.0f, -1.0f, 1.0f);
    // Center the object in the window
    glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(width_ / 2.0f, height_ / 2.0f, 0.0f));
    model = glm::scale(model, glm::vec3(radius, radius, 1.0f)); // Scale uniformly in 2D
    glm::mat4 view = glm::mat4(1.0f); // No view transformation for 2D

    PushConstants pushConstants = {model, view, ortho, radius, wavePhase_};
    vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0, sizeof(PushConstants), &pushConstants);

    VkBuffer vertexBuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffers_[imageIndex], indexBuffer_, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);
}

void renderMode2(uint32_t imageIndex) {
    struct PushConstants {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 proj;
        float radius;
        float wavePhase;
    };

    VkBuffer vertexBuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffers_[imageIndex], indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

    // 3D perspective projection
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width_ / height_, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(0.0f, 0.0f, 5.0f), glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    float baseRadius = 1.0f;
    for (size_t i = 0; i < cache_.size(); ++i) {
        // Use cached UniversalEquation data for each dimension
        float radius = baseRadius * (1.0f + static_cast<float>(cache_[i].positive) * 0.1f);
        // Add rotation for 3D effect
        glm::mat4 model = glm::rotate(glm::mat4(1.0f), wavePhase_ * 0.5f, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::scale(model, glm::vec3(radius));
        // Offset spheres slightly along z-axis for visibility
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, -i * 0.5f));

        PushConstants pushConstants = {model, view, proj, radius, wavePhase_};
        vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                           0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);
        baseRadius *= 0.8f; // Reduce radius for higher dimensions
    }
}
};

#endif // MAIN_HPP