#ifndef MAIN_HPP
#define MAIN_HPP

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <vector>
#include <iostream>
#include <map>
#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "vulkan/vulkan.h"
#include "SDL3_init.hpp"
#include "Vulkan_init.hpp"
#include "universal_equation.hpp"
#include "types.hpp"
#include "modes.hpp"

class DimensionalNavigator {
public:
    DimensionalNavigator(const char* title = "Dimensional Navigator",
                        int width = 1280, int height = 720)
        : window_(nullptr), vulkanInstance_(VK_NULL_HANDLE), vulkanDevice_(VK_NULL_HANDLE),
          physicalDevice_(VK_NULL_HANDLE), surface_(VK_NULL_HANDLE), swapchain_(VK_NULL_HANDLE),
          mode_(1), wavePhase_(0.0f), waveSpeed_(0.1f), width_(width), height_(height),
          zoomLevel_(1.0f), isPaused_(false), isSwapchainValid_(true) {
        try {
            SDL3Initializer::initializeSDL(window_, vulkanInstance_, surface_, title, width, height);
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

    SDL_Window* window_;
    VkInstance vulkanInstance_;
    VkDevice vulkanDevice_;
    VkPhysicalDevice physicalDevice_;
    glm::vec3 userCamPos_ = glm::vec3(0.0f, 0.0f, 10.0f);
    bool isUserCamActive_ = false;
    VkSurfaceKHR surface_;
    VkBuffer quadVertexBuffer_;
    VkDeviceMemory quadVertexBufferMemory_;
    VkBuffer quadIndexBuffer_;
    VkDeviceMemory quadIndexBufferMemory_;
    std::vector<VkCommandBuffer> commandBuffers_;
    VkSwapchainKHR swapchain_;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> swapchainFramebuffers_;
    VkQueue graphicsQueue_;
    VkQueue presentQueue_;
    VkPipeline pipeline_;
    VkPipelineLayout pipelineLayout_;
    VkRenderPass renderPass_;
    VkCommandPool commandPool_;
    VkSemaphore imageAvailableSemaphore_;
    VkSemaphore renderFinishedSemaphore_;
    VkFence inFlightFence_;
    VkBuffer vertexBuffer_;
    VkDeviceMemory vertexBufferMemory_;
    VkBuffer indexBuffer_;
    VkDeviceMemory indexBufferMemory_;
    UniversalEquation ue_;
    std::vector<DimensionData> cache_;
    std::vector<glm::vec3> sphereVertices_;
    std::vector<uint32_t> sphereIndices_;
    std::vector<glm::vec3> quadVertices_;
    std::vector<uint32_t> quadIndices_;
    uint32_t graphicsFamily_ = UINT32_MAX;
    uint32_t presentFamily_ = UINT32_MAX;
    static constexpr int kMaxRenderedDimensions = 9;
    int mode_;
    float wavePhase_;
    const float waveSpeed_;
    int width_;
    int height_;
    float zoomLevel_;
    bool isPaused_;
    bool isSwapchainValid_; // Added to track swapchain validity

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
            if (!isPaused_ && isSwapchainValid_) { // Only render if not paused and swapchain is valid
                try {
                    render();
                } catch (const std::exception& e) {
                    std::cerr << "Render failed: " << e.what() << "\n";
                    running = false;
                }
            }
            SDL_Delay(16); // ~60 FPS
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

    void updateZoom(bool zoomIn) {
        if (zoomIn) {
            zoomLevel_ = std::max(0.01f, zoomLevel_ * 0.9f);
        } else {
            zoomLevel_ = std::min(20.0f, zoomLevel_ * 1.1f);
        }
    }

    void handleInput(const SDL_Event& event) {
        if (event.type == SDL_EVENT_KEY_DOWN) {
            switch (event.key.key) {
                case SDLK_F: {
                    Uint32 flags = SDL_GetWindowFlags(window_);
                    if (flags & SDL_WINDOW_FULLSCREEN) {
                        SDL_SetWindowFullscreen(window_, 0);
                    } else {
                        SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN);
                    }
                    break;
                }
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
                case SDLK_5:
                    mode_ = 5;
                    break;
                case SDLK_6:
                    mode_ = 6;
                    break;
                case SDLK_7:
                    mode_ = 7;
                    break;
                case SDLK_8:
                    mode_ = 8;
                    break;
                case SDLK_9:
                    mode_ = 9;
                    break;
                case SDLK_A:
                    updateZoom(true);
                    break;
                case SDLK_Z:
                    updateZoom(false);
                    break;
                case SDLK_P:
                    isPaused_ = !isPaused_;
                    std::cerr << "Pause state: " << (isPaused_ ? "Paused" : "Unpaused") << "\n";
                    break;
            }
        }
    }

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
        isSwapchainValid_ = true; // Mark swapchain as valid after initialization
    }

    void recreateSwapchain() {
        // Mark swapchain as invalid
        isSwapchainValid_ = false;

        // Check for minimized window
        int newWidth, newHeight;
        SDL_GetWindowSize(window_, &newWidth, &newHeight);
        if (newWidth == 0 || newHeight == 0) {
            std::cerr << "Window is minimized, skipping swapchain recreation\n";
            return;
        }

        // Wait for device to be idle before cleanup
        if (vulkanDevice_ != VK_NULL_HANDLE) {
            VkResult result = vkDeviceWaitIdle(vulkanDevice_);
            if (result != VK_SUCCESS) {
                std::cerr << "vkDeviceWaitIdle failed: " << result << "\n";
            }
        }

        // Cleanup Vulkan resources
        std::cerr << "Cleaning up Vulkan resources\n";
        VulkanInitializer::cleanupVulkan(vulkanInstance_, vulkanDevice_, surface_, swapchain_, swapchainImageViews_,
                                        swapchainFramebuffers_, pipeline_, pipelineLayout_, renderPass_,
                                        commandPool_, commandBuffers_, imageAvailableSemaphore_, renderFinishedSemaphore_,
                                        inFlightFence_, vertexBuffer_, vertexBufferMemory_, indexBuffer_, indexBufferMemory_,
                                        quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_, quadIndexBufferMemory_);

        // Nullify resources after cleanup (except vulkanDevice_)
        std::cerr << "Nullifying Vulkan resources\n";
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

        // Update window size
        width_ = std::max(1280, newWidth); // Enforce minimum width
        height_ = std::max(720, newHeight); // Enforce minimum height
        std::cerr << "Recreating swapchain with resolution: " << width_ << "x" << height_ << "\n";
        if (newWidth < 1280 || newHeight < 720) {
            SDL_SetWindowSize(window_, width_, height_);
            std::cerr << "Adjusted window size to: " << width_ << "x" << height_ << "\n";
        }

        // Reinitialize Vulkan
        try {
            initializeVulkan();
        } catch (const std::exception& e) {
            std::cerr << "Failed to reinitialize Vulkan: " << e.what() << "\n";
            throw;
        }
    }

    void cleanup() {
        // Wait for device to be idle before cleanup
        if (vulkanDevice_ != VK_NULL_HANDLE) {
            VkResult result = vkDeviceWaitIdle(vulkanDevice_);
            if (result != VK_SUCCESS) {
                std::cerr << "vkDeviceWaitIdle failed in cleanup: " << result << "\n";
            }
        }

        // Cleanup Vulkan resources
        std::cerr << "Cleaning up Vulkan resources in cleanup\n";
        VulkanInitializer::cleanupVulkan(vulkanInstance_, vulkanDevice_, surface_, swapchain_, swapchainImageViews_,
                                        swapchainFramebuffers_, pipeline_, pipelineLayout_, renderPass_,
                                        commandPool_, commandBuffers_, imageAvailableSemaphore_, renderFinishedSemaphore_,
                                        inFlightFence_, vertexBuffer_, vertexBufferMemory_, indexBuffer_, indexBufferMemory_,
                                        quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_, quadIndexBufferMemory_);

        // Cleanup SDL resources
        SDL3Initializer::cleanupSDL(window_, vulkanInstance_, surface_);
        window_ = nullptr;
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
            float sinPhi = sinf(phi);
            float cosPhi = cosf(phi);
            for (int j = 0; j <= slices; ++j) {
                float theta = j * 2.0f * M_PI / slices;
                float sinTheta = sinf(theta);
                float cosTheta = cosf(theta);
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
        ue_ = UniversalEquation(9, 1, 1.0, 0.5, 0.5, 0.5, 1.5, 2.0, 0.27, 0.68, 5.0, 0.2, true);
        updateCache();
    }

    void updateCache() {
        cache_.clear();
        cache_.reserve(kMaxRenderedDimensions);
        for (int d = 1; d <= kMaxRenderedDimensions; ++d) {
            ue_.setCurrentDimension(d);
            auto result = ue_.compute();
            cache_.push_back({d, result.observable, result.potential, result.darkMatter, result.darkEnergy});
            if (ue_.getDebug()) {
                std::cerr << "Cache[D=" << d << "]: " << result.toString() << "\n";
            }
        }
    }

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

    void render() {
        if (!isSwapchainValid_) {
            std::cerr << "Swapchain is invalid, skipping render\n";
            return;
        }

        std::cerr << "Rendering with resolution: " << width_ << "x" << height_ << ", aspect ratio: " << static_cast<float>(width_) / height_ << "\n";
        vkWaitForFences(vulkanDevice_, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
        vkResetFences(vulkanDevice_, 1, &inFlightFence_);

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vulkanDevice_, swapchain_, UINT64_MAX, imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            recreateSwapchain();
            return;
        }
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to acquire swapchain image: " + std::to_string(result));
        }

        vkResetCommandBuffer(commandBuffers_[imageIndex], 0);

        VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr};
        if (vkBeginCommandBuffer(commandBuffers_[imageIndex], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin command buffer");
        }

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkRenderPassBeginInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, renderPass_, swapchainFramebuffers_[imageIndex], {{0, 0}, {(uint32_t)width_, (uint32_t)height_}}, 1, &clearColor};
        vkCmdBeginRenderPass(commandBuffers_[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffers_[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

        // Set viewport and scissor
        VkViewport viewport = {0.0f, 0.0f, (float)width_, (float)height_, 0.0f, 1.0f};
        vkCmdSetViewport(commandBuffers_[imageIndex], 0, 1, &viewport);

        VkRect2D scissor = {{0, 0}, {(uint32_t)width_, (uint32_t)height_}};
        vkCmdSetScissor(commandBuffers_[imageIndex], 0, 1, &scissor);

        updateCache();
        switch (mode_) {
            case 1: renderMode1(this, imageIndex); break;
            case 2: renderMode2(this, imageIndex, vertexBuffer_, commandBuffers_, indexBuffer_, zoomLevel_, width_, height_, wavePhase_, cache_); break;
            case 3: renderMode3(this, imageIndex, vertexBuffer_, commandBuffers_, indexBuffer_, zoomLevel_, width_, height_, wavePhase_, cache_); break;
            case 4: renderMode4(this, imageIndex, vertexBuffer_, commandBuffers_, indexBuffer_, zoomLevel_, width_, height_, wavePhase_, cache_); break;
            case 5: renderMode5(this, imageIndex, vertexBuffer_, commandBuffers_, indexBuffer_, zoomLevel_, width_, height_, wavePhase_, cache_); break;
            case 6: renderMode6(this, imageIndex, vertexBuffer_, commandBuffers_, indexBuffer_, zoomLevel_, width_, height_, wavePhase_, cache_); break;
            case 7: renderMode7(this, imageIndex, vertexBuffer_, commandBuffers_, indexBuffer_, zoomLevel_, width_, height_, wavePhase_, cache_); break;
            case 8: renderMode8(this, imageIndex, vertexBuffer_, commandBuffers_, indexBuffer_, zoomLevel_, width_, height_, wavePhase_, cache_); break;
            case 9: renderMode9(this, imageIndex, vertexBuffer_, commandBuffers_, indexBuffer_, zoomLevel_, width_, height_, wavePhase_, cache_); break;
            default: renderMode1(this, imageIndex);
        }

        vkCmdEndRenderPass(commandBuffers_[imageIndex]);
        if (vkEndCommandBuffer(commandBuffers_[imageIndex]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to end command buffer");
        }

        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1, &imageAvailableSemaphore_, waitStages, 1, &commandBuffers_[imageIndex], 1, &renderFinishedSemaphore_};
        if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFence_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit queue");
        }

        VkPresentInfoKHR presentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1, &renderFinishedSemaphore_, 1, &swapchain_, &imageIndex, nullptr};
        result = vkQueuePresentKHR(presentQueue_, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
            recreateSwapchain();
            return;
        }
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present queue: " + std::to_string(result));
        }

        wavePhase_ += waveSpeed_;
    }
};

#endif // MAIN_HPP