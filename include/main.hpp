#ifndef MAIN_HPP
#define MAIN_HPP

// AMOURANTH RTX engine, Sep 2025 - Main Application class header.
// Manages SDL3 window/input, Vulkan rendering, and engine logic (DimensionalNavigator, AMOURANTH).
// Features: Thread-safe (OpenMP), memory-safe, error handling (std::runtime_error), Vulkan 1.2+ with ray tracing.
// Usage: Application app("Title", 1920, 1080); app.run();

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <iostream>
#include <omp.h>
#include <vector>
#include "SDL3_init.hpp"
#include "Vulkan_init.hpp"
#include "core.hpp"

class DimensionalNavigator;
class AMOURANTH;

class Application {
public:
    // Initializes SDL3 (window, Vulkan instance, surface), Vulkan resources, and engine (simulator, renderer).
    // Parameters: title (default: "Dimensional Navigator"), width/height (default: 1280x720; 0x0 on Android for fullscreen).
    // Throws: std::runtime_error on init failure.
    Application(const char* title = "Dimensional Navigator", int width = 1280, int height = 720)
        : simulator_(nullptr), sdlInitializer_(), window_(nullptr), vulkanInstance_(VK_NULL_HANDLE),
          vulkanDevice_(VK_NULL_HANDLE), physicalDevice_(VK_NULL_HANDLE), surface_(VK_NULL_HANDLE),
          descriptorSetLayout_(VK_NULL_HANDLE), quadVertexBuffer_(VK_NULL_HANDLE), quadVertexBufferMemory_(VK_NULL_HANDLE),
          quadIndexBuffer_(VK_NULL_HANDLE), quadIndexBufferMemory_(VK_NULL_HANDLE), commandBuffers_(),
          swapchain_(VK_NULL_HANDLE), swapchainImages_(), swapchainImageViews_(), swapchainFramebuffers_(),
          graphicsQueue_(VK_NULL_HANDLE), presentQueue_(VK_NULL_HANDLE), pipeline_(VK_NULL_HANDLE),
          pipelineLayout_(VK_NULL_HANDLE), renderPass_(VK_NULL_HANDLE), commandPool_(VK_NULL_HANDLE),
          imageAvailableSemaphores_(), renderFinishedSemaphores_(), inFlightFences_(),
          vertexBuffer_(VK_NULL_HANDLE), vertexBufferMemory_(VK_NULL_HANDLE), indexBuffer_(VK_NULL_HANDLE),
          indexBufferMemory_(VK_NULL_HANDLE), graphicsFamily_(UINT32_MAX), presentFamily_(UINT32_MAX),
          width_(width), height_(height), amouranth_(nullptr) {
        try {
            omp_set_num_threads(omp_get_max_threads()); // Max OpenMP threads for parallelism.
            sdlInitializer_.initialize(title, width, height); // SDL3 init: window, instance, surface.
            window_ = sdlInitializer_.getWindow();
            vulkanInstance_ = sdlInitializer_.getVkInstance();
            surface_ = sdlInitializer_.getVkSurface();

#ifdef __ANDROID__
            if (width == 0 || height == 0) SDL_GetWindowSize(window_, &width_, &height_); // Query Android fullscreen size.
#endif

            simulator_ = new DimensionalNavigator("Dimensional Navigator", width_, height_);
            amouranth_ = new AMOURANTH(simulator_);
            initializeVulkan(); // Vulkan init: device, swapchain, pipeline, buffers.
            amouranth_->setMode(9); // Default mode.
            amouranth_->updateCache();
        } catch (const std::exception& e) {
            std::cerr << "Init failed: " << e.what() << "\n";
            cleanup();
            throw;
        }
    }

    // Cleans up all resources (Vulkan, SDL3, engine). Idempotent.
    ~Application() { cleanup(); }

    // Non-copyable/movable due to Vulkan resource constraints.
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    // Runs SDL3 event loop with render, resize, input callbacks. Fullscreen enabled.
    // Throws: std::runtime_error on critical failures (e.g., swapchain recreation).
    void run() {
        sdlInitializer_.eventLoop(
            [this]() { render(); }, // Render each frame (~60 FPS).
            [this](int w, int h) { // Resize: Update dimensions, recreate swapchain.
#ifdef __ANDROID__
                width_ = w; height_ = h;
#else
                width_ = std::max(1280, w); height_ = std::max(720, h); // Clamp desktop size.
#endif
                try { recreateSwapchain(); } catch (const std::exception& e) {
                    std::cerr << "Swapchain recreation failed: " << e.what() << "\n";
                    throw;
                }
            },
            true, // Fullscreen.
            [this](const SDL_KeyboardEvent& key) { amouranth_->handleInput(key); } // Input handling.
        );
    }

private:
    DimensionalNavigator* simulator_; // Simulator (physics, modes).
    SDL3Initializer sdlInitializer_; // SDL3 window, instance, surface.
    SDL_Window* window_;
    VkInstance vulkanInstance_;
    VkDevice vulkanDevice_;
    VkPhysicalDevice physicalDevice_;
    VkSurfaceKHR surface_;
    VkDescriptorSetLayout descriptorSetLayout_;
    VkBuffer quadVertexBuffer_;
    VkDeviceMemory quadVertexBufferMemory_;
    VkBuffer quadIndexBuffer_;
    VkDeviceMemory quadIndexBufferMemory_;
    std::vector<VkCommandBuffer> commandBuffers_;
    VkSwapchainKHR swapchain_;
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> swapchainFramebuffers_;
    VkQueue graphicsQueue_, presentQueue_;
    VkPipeline pipeline_;
    VkPipelineLayout pipelineLayout_;
    VkRenderPass renderPass_;
    VkCommandPool commandPool_;
    std::vector<VkSemaphore> imageAvailableSemaphores_, renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    VkBuffer vertexBuffer_;
    VkDeviceMemory vertexBufferMemory_;
    VkBuffer indexBuffer_;
    VkDeviceMemory indexBufferMemory_;
    uint32_t graphicsFamily_, presentFamily_;
    int width_, height_;
    AMOURANTH* amouranth_; // Renderer (visuals, input).

    // Initializes Vulkan: device, queues, swapchain, pipeline, buffers, sync objects.
    // Throws: std::runtime_error on failure.
    void initializeVulkan() {
        VulkanInit::initializeVulkan(
            vulkanInstance_, physicalDevice_, vulkanDevice_, surface_, graphicsQueue_, presentQueue_,
            graphicsFamily_, presentFamily_, swapchain_, swapchainImages_, swapchainImageViews_,
            renderPass_, pipeline_, pipelineLayout_, descriptorSetLayout_, swapchainFramebuffers_,
            commandPool_, commandBuffers_, imageAvailableSemaphores_, renderFinishedSemaphores_,
            inFlightFences_, vertexBuffer_, vertexBufferMemory_, indexBuffer_, indexBufferMemory_,
            amouranth_->getSphereVertices(), amouranth_->getSphereIndices(), width_, height_);
        VulkanInit::initializeQuadBuffers(
            vulkanDevice_, physicalDevice_, commandPool_, graphicsQueue_,
            quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_, quadIndexBufferMemory_,
            amouranth_->getQuadVertices(), amouranth_->getQuadIndices());
    }

    // Recreates swapchain on resize. Waits for device idle, cleans up, reinitializes Vulkan.
    // Throws: std::runtime_error on failure.
    void recreateSwapchain() {
        if (vulkanDevice_ != VK_NULL_HANDLE) {
            VkResult result = vkDeviceWaitIdle(vulkanDevice_);
            if (result != VK_SUCCESS) std::cerr << "vkDeviceWaitIdle failed: " << result << "\n";
        }
        VulkanInit::cleanupVulkan(
            vulkanInstance_, vulkanDevice_, surface_, swapchain_, swapchainImageViews_,
            swapchainFramebuffers_, pipeline_, pipelineLayout_, renderPass_, commandPool_,
            commandBuffers_, imageAvailableSemaphores_, renderFinishedSemaphores_, inFlightFences_,
            vertexBuffer_, vertexBufferMemory_, indexBuffer_, indexBufferMemory_,
            quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_, quadIndexBufferMemory_,
            descriptorSetLayout_);
        descriptorSetLayout_ = VK_NULL_HANDLE;
        swapchain_ = VK_NULL_HANDLE;
        swapchainImages_.clear();
        swapchainImageViews_.clear();
        swapchainFramebuffers_.clear();
        pipeline_ = VK_NULL_HANDLE;
        pipelineLayout_ = VK_NULL_HANDLE;
        renderPass_ = VK_NULL_HANDLE;
        commandPool_ = VK_NULL_HANDLE;
        commandBuffers_.clear();
        imageAvailableSemaphores_.clear();
        renderFinishedSemaphores_.clear();
        inFlightFences_.clear();
        vertexBuffer_ = VK_NULL_HANDLE;
        vertexBufferMemory_ = VK_NULL_HANDLE;
        indexBuffer_ = VK_NULL_HANDLE;
        indexBufferMemory_ = VK_NULL_HANDLE;
        quadVertexBuffer_ = VK_NULL_HANDLE;
        quadVertexBufferMemory_ = VK_NULL_HANDLE;
        quadIndexBuffer_ = VK_NULL_HANDLE;
        quadIndexBufferMemory_ = VK_NULL_HANDLE;
        vulkanDevice_ = VK_NULL_HANDLE;
        int newWidth, newHeight;
        SDL_GetWindowSize(window_, &newWidth, &newHeight);
        width_ = newWidth; height_ = newHeight;
#ifndef __ANDROID__
        width_ = std::max(1280, newWidth); height_ = std::max(720, newHeight);
        if (newWidth < 1280 || newHeight < 720) SDL_SetWindowSize(window_, width_, height_);
#endif
        initializeVulkan();
    }

    // Cleans up all resources (Vulkan, SDL3, engine). Idempotent, no throws.
    void cleanup() {
        if (vulkanDevice_ != VK_NULL_HANDLE) {
            VkResult result = vkDeviceWaitIdle(vulkanDevice_);
            if (result != VK_SUCCESS) std::cerr << "vkDeviceWaitIdle failed: " << result << "\n";
        }
        VulkanInit::cleanupVulkan(
            vulkanInstance_, vulkanDevice_, surface_, swapchain_, swapchainImageViews_,
            swapchainFramebuffers_, pipeline_, pipelineLayout_, renderPass_, commandPool_,
            commandBuffers_, imageAvailableSemaphores_, renderFinishedSemaphores_, inFlightFences_,
            vertexBuffer_, vertexBufferMemory_, indexBuffer_, indexBufferMemory_,
            quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_, quadIndexBufferMemory_,
            descriptorSetLayout_);
        sdlInitializer_.cleanup();
        delete amouranth_; delete simulator_;
        amouranth_ = nullptr; simulator_ = nullptr; window_ = nullptr;
        vulkanInstance_ = VK_NULL_HANDLE; surface_ = VK_NULL_HANDLE;
        descriptorSetLayout_ = VK_NULL_HANDLE;
        imageAvailableSemaphores_.clear(); renderFinishedSemaphores_.clear(); inFlightFences_.clear();
    }

    // Renders one frame. Skips if user camera active. Updates simulation, records commands, submits/presents.
    // Throws: std::runtime_error on critical failures.
    void render() {
        if (amouranth_->isUserCamActive()) return;
        amouranth_->update(0.016f); // ~60 FPS.
        vkWaitForFences(vulkanDevice_, 1, &inFlightFences_[0], VK_TRUE, UINT64_MAX);
        vkResetFences(vulkanDevice_, 1, &inFlightFences_[0]);
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vulkanDevice_, swapchain_, UINT64_MAX, imageAvailableSemaphores_[0], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain(); return; }
        if (result != VK_SUCCESS) throw std::runtime_error("Failed to acquire swapchain image: " + std::to_string(result));
        vkResetCommandBuffer(commandBuffers_[imageIndex], 0);
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr };
        if (vkBeginCommandBuffer(commandBuffers_[imageIndex], &beginInfo) != VK_SUCCESS) throw std::runtime_error("Failed to begin command buffer");
        VkClearValue clearColor = { {{0.1f, 0.1f, 0.1f, 1.0f}} };
        VkRenderPassBeginInfo renderPassInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, renderPass_, swapchainFramebuffers_[imageIndex],
            {{0, 0}, {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_)}}, 1, &clearColor
        };
        vkCmdBeginRenderPass(commandBuffers_[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffers_[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        amouranth_->render(imageIndex, vertexBuffer_, commandBuffers_[imageIndex], indexBuffer_, pipelineLayout_);
        vkCmdEndRenderPass(commandBuffers_[imageIndex]);
        if (vkEndCommandBuffer(commandBuffers_[imageIndex]) != VK_SUCCESS) throw std::runtime_error("Failed to end command buffer");
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSubmitInfo submitInfo = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1, &imageAvailableSemaphores_[0], waitStages,
            1, &commandBuffers_[imageIndex], 1, &renderFinishedSemaphores_[0]
        };
        if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[0]) != VK_SUCCESS) throw std::runtime_error("Failed to submit queue");
        VkPresentInfoKHR presentInfo = {
            VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1, &renderFinishedSemaphores_[0], 1, &swapchain_, &imageIndex, nullptr
        };
        result = vkQueuePresentKHR(presentQueue_, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain(); return; }
        if (result != VK_SUCCESS) throw std::runtime_error("Failed to present queue: " + std::to_string(result));
    }
};

#endif // MAIN_HPP