#ifndef MAIN_HPP
#define MAIN_HPP

// AMOURANTH RTX Engine, Sep 2025 - Main Application class header.
// Manages SDL3 window/input, Vulkan rendering, and engine logic (DimensionalNavigator, AMOURANTH).
// Features: Thread-safe (OpenMP, mutexes), memory-safe, error handling (std::runtime_error), Vulkan 1.2+ with ray tracing.
// Input handling is managed via HandleInput (handleinput.hpp) for modularity.
// Font handling is managed via SDL3Font (SDL3_font.hpp in SDL3Initializer).
// Usage: Application app("Title", 1920, 1080); app.run();
// Fixed: Updated initializeVulkan to avoid redundant sync object creation, resolving VUID-vkDestroyDevice-device-05137.
// Fixed: Updated render to cycle semaphores/fences with currentFrame, resolving VUID-vkQueueSubmit-pSignalSemaphores-00067.
// Fixed: Updated cleanup and recreateSwapchain to include descriptorPool, descriptorSet, sampler, and shader modules,
// resolving signature mismatch with VulkanInitializer::cleanupVulkan.
// Fixed: Moved quad buffer destruction before VulkanInitializer::cleanupVulkan to resolve VUID-vkDestroyDevice-device-05137.
// Fixed: Removed instance and surface destruction from VulkanInitializer::cleanupVulkan to avoid double destruction,
// resolving VUID-vkDestroySurfaceKHR-instance-parameter.
// Fixed: Marked methods as inline to resolve undefined reference linker errors.
// Fixed: Integrated HandleInput for input processing, supporting all SDL3 input types.
// Updated: Added mutexes for thread-safe access to shared resources and Vulkan objects, excluding recreateSwapchain to avoid resizing freezes.
// Fixed: Use std::span directly for VulkanInitializer calls to match updated signatures.
// Zachary Geurts, 2025

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <iostream>
#include <omp.h>
#include <vector>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "engine/SDL3_init.hpp"
#include "engine/Vulkan_init.hpp"
#include "engine/core.hpp"
#include "handleinput.hpp"

class DimensionalNavigator;
class AMOURANTH;
class HandleInput;

class Application {
public:
    Application(const char* title = "Dimensional Navigator", int width = 1280, int height = 720)
        : simulator_(nullptr), sdlInitializer_(), window_(nullptr), vulkanInstance_(VK_NULL_HANDLE),
          vulkanDevice_(VK_NULL_HANDLE), physicalDevice_(VK_NULL_HANDLE), surface_(VK_NULL_HANDLE),
          descriptorSetLayout_(VK_NULL_HANDLE), descriptorPool_(VK_NULL_HANDLE), descriptorSet_(VK_NULL_HANDLE),
          sampler_(VK_NULL_HANDLE), quadVertexBuffer_(VK_NULL_HANDLE), quadVertexBufferMemory_(VK_NULL_HANDLE),
          quadIndexBuffer_(VK_NULL_HANDLE), quadIndexBufferMemory_(VK_NULL_HANDLE),
          sphereStagingBuffer_(VK_NULL_HANDLE), sphereStagingBufferMemory_(VK_NULL_HANDLE),
          indexStagingBuffer_(VK_NULL_HANDLE), indexStagingBufferMemory_(VK_NULL_HANDLE),
          quadStagingBuffer_(VK_NULL_HANDLE), quadStagingBufferMemory_(VK_NULL_HANDLE),
          quadIndexStagingBuffer_(VK_NULL_HANDLE), quadIndexStagingBufferMemory_(VK_NULL_HANDLE),
          vertShaderModule_(VK_NULL_HANDLE), fragShaderModule_(VK_NULL_HANDLE),
          commandBuffers_(), swapchain_(VK_NULL_HANDLE), swapchainImages_(), swapchainImageViews_(),
          swapchainFramebuffers_(), graphicsQueue_(VK_NULL_HANDLE), presentQueue_(VK_NULL_HANDLE),
          pipeline_(VK_NULL_HANDLE), pipelineLayout_(VK_NULL_HANDLE), renderPass_(VK_NULL_HANDLE),
          commandPool_(VK_NULL_HANDLE), imageAvailableSemaphores_(), renderFinishedSemaphores_(),
          inFlightFences_(), vertexBuffer_(VK_NULL_HANDLE), vertexBufferMemory_(VK_NULL_HANDLE),
          indexBuffer_(VK_NULL_HANDLE), indexBufferMemory_(VK_NULL_HANDLE),
          graphicsFamily_(UINT32_MAX), presentFamily_(UINT32_MAX), width_(width), height_(height),
          amouranth_(nullptr), input_(nullptr) {
        try {
            omp_set_num_threads(omp_get_max_threads());
            sdlInitializer_.initialize(title, width, height);
            window_ = sdlInitializer_.getWindow();
            vulkanInstance_ = sdlInitializer_.getVkInstance();
            surface_ = sdlInitializer_.getVkSurface();

#ifdef __ANDROID__
            if (width == 0 || height == 0) SDL_GetWindowSize(window_, &width_, &height_);
#endif

            simulator_ = new DimensionalNavigator("Dimensional Navigator", width_, height_);
            amouranth_ = new AMOURANTH(simulator_);
            input_ = new HandleInput(amouranth_, simulator_);
            input_->setCallbacks(); // Set default input handlers
            initializeVulkan();
            amouranth_->setMode(9);
            amouranth_->updateCache();
        } catch (const std::exception& e) {
            std::cerr << "Init failed: " << e.what() << "\n";
            cleanup();
            throw;
        }
    }

    ~Application() { cleanup(); }

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    inline void run() {
        sdlInitializer_.eventLoop(
            [this]() { render(); },
            [this](int w, int h) {
#ifdef __ANDROID__
                width_ = w; height_ = h;
#else
                width_ = std::max(1280, w); height_ = std::max(720, h);
#endif
                try { recreateSwapchain(); } catch (const std::exception& e) {
                    std::cerr << "Swapchain recreation failed: " << e.what() << "\n";
                    throw;
                }
            },
            true,
            input_->getKeyboardCallback(),
            input_->getMouseButtonCallback(),
            input_->getMouseMotionCallback(),
            input_->getMouseWheelCallback(),
            input_->getTextInputCallback(),
            input_->getTouchCallback(),
            input_->getGamepadButtonCallback(),
            input_->getGamepadAxisCallback(),
            input_->getGamepadConnectCallback()
        );
    }

private:
    DimensionalNavigator* simulator_;
    SDL3Initializer::SDL3Initializer sdlInitializer_;
    SDL_Window* window_;
    VkInstance vulkanInstance_;
    VkDevice vulkanDevice_;
    VkPhysicalDevice physicalDevice_;
    VkSurfaceKHR surface_;
    VkDescriptorSetLayout descriptorSetLayout_;
    VkDescriptorPool descriptorPool_;
    VkDescriptorSet descriptorSet_;
    VkSampler sampler_;
    VkBuffer quadVertexBuffer_;
    VkDeviceMemory quadVertexBufferMemory_;
    VkBuffer quadIndexBuffer_;
    VkDeviceMemory quadIndexBufferMemory_;
    VkBuffer sphereStagingBuffer_;
    VkDeviceMemory sphereStagingBufferMemory_;
    VkBuffer indexStagingBuffer_;
    VkDeviceMemory indexStagingBufferMemory_;
    VkBuffer quadStagingBuffer_;
    VkDeviceMemory quadStagingBufferMemory_;
    VkBuffer quadIndexStagingBuffer_;
    VkDeviceMemory quadIndexStagingBufferMemory_;
    VkShaderModule vertShaderModule_;
    VkShaderModule fragShaderModule_;
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
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    VkBuffer vertexBuffer_;
    VkDeviceMemory vertexBufferMemory_;
    VkBuffer indexBuffer_;
    VkDeviceMemory indexBufferMemory_;
    uint32_t graphicsFamily_;
    uint32_t presentFamily_;
    int width_, height_;
    AMOURANTH* amouranth_;
    HandleInput* input_;
    std::mutex resourceMutex_; // Protects shared simulation/rendering data
    std::mutex vulkanMutex_;   // Protects Vulkan resource access

    inline void initializeVulkan() {
        VulkanInitializer::initializeVulkan(
            vulkanInstance_, physicalDevice_, vulkanDevice_, surface_, graphicsQueue_, presentQueue_,
            graphicsFamily_, presentFamily_, swapchain_, swapchainImages_, swapchainImageViews_,
            renderPass_, pipeline_, pipelineLayout_, descriptorSetLayout_, swapchainFramebuffers_,
            commandPool_, commandBuffers_, imageAvailableSemaphores_, renderFinishedSemaphores_,
            inFlightFences_, vertexBuffer_, vertexBufferMemory_, indexBuffer_, indexBufferMemory_,
            sphereStagingBuffer_, sphereStagingBufferMemory_, indexStagingBuffer_, indexStagingBufferMemory_,
            descriptorSetLayout_, descriptorPool_, descriptorSet_, sampler_,
            vertShaderModule_, fragShaderModule_,
            amouranth_->getSphereVertices(), amouranth_->getSphereIndices(), width_, height_);

        VulkanInitializer::initializeQuadBuffers(
            vulkanDevice_, physicalDevice_, commandPool_, graphicsQueue_,
            quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_, quadIndexBufferMemory_,
            quadStagingBuffer_, quadStagingBufferMemory_, quadIndexStagingBuffer_, quadIndexStagingBufferMemory_,
            amouranth_->getQuadVertices(), amouranth_->getQuadIndices());
    }

    inline void recreateSwapchain() {
        if (vulkanDevice_ != VK_NULL_HANDLE) {
            VkResult result = vkDeviceWaitIdle(vulkanDevice_);
            if (result != VK_SUCCESS) std::cerr << "vkDeviceWaitIdle failed: " << result << "\n";
        }
        // Destroy quad buffers before cleanupVulkan
        if (quadVertexBuffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(vulkanDevice_, quadVertexBuffer_, nullptr);
            quadVertexBuffer_ = VK_NULL_HANDLE;
        }
        if (quadVertexBufferMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(vulkanDevice_, quadVertexBufferMemory_, nullptr);
            quadVertexBufferMemory_ = VK_NULL_HANDLE;
        }
        if (quadIndexBuffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(vulkanDevice_, quadIndexBuffer_, nullptr);
            quadIndexBuffer_ = VK_NULL_HANDLE;
        }
        if (quadIndexBufferMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(vulkanDevice_, quadIndexBufferMemory_, nullptr);
            quadIndexBufferMemory_ = VK_NULL_HANDLE;
        }
        if (quadStagingBuffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(vulkanDevice_, quadStagingBuffer_, nullptr);
            quadStagingBuffer_ = VK_NULL_HANDLE;
        }
        if (quadStagingBufferMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(vulkanDevice_, quadStagingBufferMemory_, nullptr);
            quadStagingBufferMemory_ = VK_NULL_HANDLE;
        }
        if (quadIndexStagingBuffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(vulkanDevice_, quadIndexStagingBuffer_, nullptr);
            quadIndexStagingBuffer_ = VK_NULL_HANDLE;
        }
        if (quadIndexStagingBufferMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(vulkanDevice_, quadIndexStagingBufferMemory_, nullptr);
            quadIndexStagingBufferMemory_ = VK_NULL_HANDLE;
        }
        VulkanInitializer::cleanupVulkan(
            vulkanDevice_, swapchain_, swapchainImageViews_,
            swapchainFramebuffers_, pipeline_, pipelineLayout_, renderPass_, commandPool_,
            commandBuffers_, imageAvailableSemaphores_, renderFinishedSemaphores_, inFlightFences_,
            vertexBuffer_, vertexBufferMemory_, indexBuffer_, indexBufferMemory_,
            descriptorSetLayout_, descriptorPool_, descriptorSet_, sampler_,
            sphereStagingBuffer_, sphereStagingBufferMemory_, indexStagingBuffer_, indexStagingBufferMemory_,
            vertShaderModule_, fragShaderModule_);
        descriptorSetLayout_ = VK_NULL_HANDLE;
        descriptorPool_ = VK_NULL_HANDLE;
        descriptorSet_ = VK_NULL_HANDLE;
        sampler_ = VK_NULL_HANDLE;
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
        sphereStagingBuffer_ = VK_NULL_HANDLE;
        sphereStagingBufferMemory_ = VK_NULL_HANDLE;
        indexStagingBuffer_ = VK_NULL_HANDLE;
        indexStagingBufferMemory_ = VK_NULL_HANDLE;
        vertShaderModule_ = VK_NULL_HANDLE;
        fragShaderModule_ = VK_NULL_HANDLE;
        int newWidth, newHeight;
        SDL_GetWindowSize(window_, &newWidth, &newHeight);
        width_ = newWidth; height_ = newHeight;
#ifndef __ANDROID__
        width_ = std::max(1280, newWidth); height_ = std::max(720, newHeight);
        if (newWidth < 1280 || newHeight < 720) SDL_SetWindowSize(window_, width_, height_);
#endif
        initializeVulkan();
    }

    inline void cleanup() {
        std::lock_guard<std::mutex> vulkanLock(vulkanMutex_);
        std::lock_guard<std::mutex> resourceLock(resourceMutex_);
        if (vulkanDevice_ != VK_NULL_HANDLE) {
            VkResult result = vkDeviceWaitIdle(vulkanDevice_);
            if (result != VK_SUCCESS) std::cerr << "vkDeviceWaitIdle failed: " << result << "\n";
        }
        // Destroy quad buffers before cleanupVulkan
        if (quadVertexBuffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(vulkanDevice_, quadVertexBuffer_, nullptr);
            quadVertexBuffer_ = VK_NULL_HANDLE;
        }
        if (quadVertexBufferMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(vulkanDevice_, quadVertexBufferMemory_, nullptr);
            quadVertexBufferMemory_ = VK_NULL_HANDLE;
        }
        if (quadIndexBuffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(vulkanDevice_, quadIndexBuffer_, nullptr);
            quadIndexBuffer_ = VK_NULL_HANDLE;
        }
        if (quadIndexBufferMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(vulkanDevice_, quadIndexBufferMemory_, nullptr);
            quadIndexBufferMemory_ = VK_NULL_HANDLE;
        }
        if (quadStagingBuffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(vulkanDevice_, quadStagingBuffer_, nullptr);
            quadStagingBuffer_ = VK_NULL_HANDLE;
        }
        if (quadStagingBufferMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(vulkanDevice_, quadStagingBufferMemory_, nullptr);
            quadStagingBufferMemory_ = VK_NULL_HANDLE;
        }
        if (quadIndexStagingBuffer_ != VK_NULL_HANDLE) {
            vkDestroyBuffer(vulkanDevice_, quadIndexStagingBuffer_, nullptr);
            quadIndexStagingBuffer_ = VK_NULL_HANDLE;
        }
        if (quadIndexStagingBufferMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(vulkanDevice_, quadIndexStagingBufferMemory_, nullptr);
            quadIndexStagingBufferMemory_ = VK_NULL_HANDLE;
        }
        VulkanInitializer::cleanupVulkan(
            vulkanDevice_, swapchain_, swapchainImageViews_,
            swapchainFramebuffers_, pipeline_, pipelineLayout_, renderPass_, commandPool_,
            commandBuffers_, imageAvailableSemaphores_, renderFinishedSemaphores_, inFlightFences_,
            vertexBuffer_, vertexBufferMemory_, indexBuffer_, indexBufferMemory_,
            descriptorSetLayout_, descriptorPool_, descriptorSet_, sampler_,
            sphereStagingBuffer_, sphereStagingBufferMemory_, indexStagingBuffer_, indexStagingBufferMemory_,
            vertShaderModule_, fragShaderModule_);
        sdlInitializer_.cleanup();
        delete input_;
        delete amouranth_;
        delete simulator_;
        input_ = nullptr;
        amouranth_ = nullptr;
        simulator_ = nullptr;
        window_ = nullptr;
        vulkanInstance_ = VK_NULL_HANDLE;
        surface_ = VK_NULL_HANDLE;
        descriptorSetLayout_ = VK_NULL_HANDLE;
        descriptorPool_ = VK_NULL_HANDLE;
        descriptorSet_ = VK_NULL_HANDLE;
        sampler_ = VK_NULL_HANDLE;
        sphereStagingBuffer_ = VK_NULL_HANDLE;
        sphereStagingBufferMemory_ = VK_NULL_HANDLE;
        indexStagingBuffer_ = VK_NULL_HANDLE;
        indexStagingBufferMemory_ = VK_NULL_HANDLE;
        quadVertexBuffer_ = VK_NULL_HANDLE;
        quadVertexBufferMemory_ = VK_NULL_HANDLE;
        quadIndexBuffer_ = VK_NULL_HANDLE;
        quadIndexBufferMemory_ = VK_NULL_HANDLE;
        quadStagingBuffer_ = VK_NULL_HANDLE;
        quadStagingBufferMemory_ = VK_NULL_HANDLE;
        quadIndexStagingBuffer_ = VK_NULL_HANDLE;
        quadIndexStagingBufferMemory_ = VK_NULL_HANDLE;
        vertShaderModule_ = VK_NULL_HANDLE;
        fragShaderModule_ = VK_NULL_HANDLE;
        imageAvailableSemaphores_.clear();
        renderFinishedSemaphores_.clear();
        inFlightFences_.clear();
    }

    inline void render() {
        static uint32_t currentFrame = 0;
        if (amouranth_->isUserCamActive()) return;
        {
            std::lock_guard<std::mutex> lock(resourceMutex_);
            amouranth_->update(0.016f);
        }
        uint32_t imageCount = static_cast<uint32_t>(swapchainImages_.size());

        {
            std::lock_guard<std::mutex> vulkanLock(vulkanMutex_);
            vkWaitForFences(vulkanDevice_, 1, &inFlightFences_[currentFrame], VK_TRUE, UINT64_MAX);
            vkResetFences(vulkanDevice_, 1, &inFlightFences_[currentFrame]);
        }

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vulkanDevice_, swapchain_, UINT64_MAX, imageAvailableSemaphores_[currentFrame], VK_NULL_HANDLE, &imageIndex);
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
        {
            std::lock_guard<std::mutex> lock(resourceMutex_);
            amouranth_->render(imageIndex, vertexBuffer_, commandBuffers_[imageIndex], indexBuffer_, pipelineLayout_, descriptorSet_);
        }
        vkCmdEndRenderPass(commandBuffers_[imageIndex]);
        if (vkEndCommandBuffer(commandBuffers_[imageIndex]) != VK_SUCCESS) throw std::runtime_error("Failed to end command buffer");

        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSubmitInfo submitInfo = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO, nullptr, 1, &imageAvailableSemaphores_[currentFrame], waitStages,
            1, &commandBuffers_[imageIndex], 1, &renderFinishedSemaphores_[currentFrame]
        };
        {
            std::lock_guard<std::mutex> vulkanLock(vulkanMutex_);
            if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFences_[currentFrame]) != VK_SUCCESS) throw std::runtime_error("Failed to submit queue");
        }

        VkPresentInfoKHR presentInfo = {
            VK_STRUCTURE_TYPE_PRESENT_INFO_KHR, nullptr, 1, &renderFinishedSemaphores_[currentFrame], 1, &swapchain_, &imageIndex, nullptr
        };
        result = vkQueuePresentKHR(presentQueue_, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain(); return; }
        if (result != VK_SUCCESS) throw std::runtime_error("Failed to present queue: " + std::to_string(result));

        currentFrame = (currentFrame + 1) % imageCount;
    }
};

#endif // MAIN_HPP