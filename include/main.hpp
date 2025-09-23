#ifndef MAIN_HPP
#define MAIN_HPP
// AMOURANTH RTX engine
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <iostream>
#include "SDL3_init.hpp"
#include "Vulkan_init.hpp"
#include "core.hpp"

// Application class managing the Vulkan rendering pipeline and SDL3 window for the Dimensional Navigator.
class Application {
public:
    // Constructor: Initializes SDL3, Vulkan, and AMOURANTH with a default window size and title.
    // Parameters:
    // - title: Window title (default: "Dimensional Navigator")
    // - width: Initial window width (default: 1280)
    // - height: Initial window height (default: 720)
    Application(const char* title = "Dimensional Navigator", int width = 1280, int height = 720)
        : simulator_(nullptr),
          sdlInitializer_(),
          window_(nullptr),
          vulkanInstance_(VK_NULL_HANDLE),
          vulkanDevice_(VK_NULL_HANDLE),
          physicalDevice_(VK_NULL_HANDLE),
          surface_(VK_NULL_HANDLE),
          swapchain_(VK_NULL_HANDLE),
          width_(width),
          height_(height),
          amouranth_(nullptr) {
        try {
            // Initialize SDL3 and create window, Vulkan instance, and surface
            sdlInitializer_.initialize(title, width, height);
            window_ = sdlInitializer_.getWindow();
            vulkanInstance_ = sdlInitializer_.getVkInstance();
            surface_ = sdlInitializer_.getVkSurface();

            // Create DimensionalNavigator and AMOURANTH
            simulator_ = new DimensionalNavigator("Dimensional Navigator", width, height);
            amouranth_ = new AMOURANTH(simulator_);

            // Initialize Vulkan resources (swapchain, pipelines, buffers, etc.)
            initializeVulkan();

            // Set initial mode to 9 (for mode9.cpp) and update cache to prevent empty cache warning
            amouranth_->setMode(9);
            amouranth_->updateCache();
        } catch (const std::exception& e) {
            std::cerr << "Initialization failed: " << e.what() << "\n";
            cleanup();
            throw;
        }
    }

    // Destructor: Cleans up all Vulkan and SDL3 resources
    ~Application() {
        cleanup();
    }

    // Main loop: Runs the SDL3 event loop and renders frames
    void run() {
        sdlInitializer_.eventLoop(
            [this]() { render(); }, // Render callback
            [this](int w, int h) {  // Resize callback
                width_ = std::max(1280, w);
                height_ = std::max(720, h);
                try {
                    recreateSwapchain();
                } catch (const std::exception& e) {
                    std::cerr << "Swapchain recreation failed: " << e.what() << "\n";
                    throw;
                }
            },
            true, // Enable resize handling
            [this](const SDL_KeyboardEvent& key) { amouranth_->handleInput(key); } // Keyboard input callback
        );
    }

private:
    DimensionalNavigator* simulator_; // Manages dimension state and cache
    SDL3Initializer sdlInitializer_; // Handles SDL3 initialization
    SDL_Window* window_;             // SDL3 window
    VkInstance vulkanInstance_;      // Vulkan instance
    VkDevice vulkanDevice_;          // Vulkan logical device
    VkPhysicalDevice physicalDevice_; // Vulkan physical device
    VkSurfaceKHR surface_;           // Vulkan surface for rendering
    VkBuffer quadVertexBuffer_;      // Vertex buffer for quad geometry (not used in mode9.cpp)
    VkDeviceMemory quadVertexBufferMemory_; // Memory for quad vertex buffer
    VkBuffer quadIndexBuffer_;       // Index buffer for quad geometry
    VkDeviceMemory quadIndexBufferMemory_; // Memory for quad index buffer
    std::vector<VkCommandBuffer> commandBuffers_; // Command buffers for rendering
    VkSwapchainKHR swapchain_;       // Vulkan swapchain
    std::vector<VkImage> swapchainImages_; // Swapchain images
    std::vector<VkImageView> swapchainImageViews_; // Image views for swapchain
    std::vector<VkFramebuffer> swapchainFramebuffers_; // Framebuffers for rendering
    VkQueue graphicsQueue_;          // Graphics queue for rendering
    VkQueue presentQueue_;           // Present queue for displaying images
    VkPipeline pipeline_;            // Graphics pipeline for sphere rendering
    VkPipelineLayout pipelineLayout_; // Pipeline layout for push constants
    VkRenderPass renderPass_;        // Render pass for framebuffers
    VkCommandPool commandPool_;      // Command pool for command buffers
    VkSemaphore imageAvailableSemaphore_; // Semaphore for swapchain image acquisition
    VkSemaphore renderFinishedSemaphore_; // Semaphore for rendering completion
    VkFence inFlightFence_;          // Fence for synchronizing frame rendering
    VkBuffer vertexBuffer_;          // Vertex buffer for sphere geometry
    VkDeviceMemory vertexBufferMemory_; // Memory for sphere vertex buffer
    VkBuffer indexBuffer_;           // Index buffer for sphere geometry
    VkDeviceMemory indexBufferMemory_; // Memory for sphere index buffer
    uint32_t graphicsFamily_ = UINT32_MAX; // Graphics queue family index
    uint32_t presentFamily_ = UINT32_MAX; // Present queue family index
    int width_;                      // Window width
    int height_;                     // Window height
    AMOURANTH* amouranth_;          // AMOURANTH instance for rendering logic

    // Initializes Vulkan resources, including swapchain, pipelines, and buffers
    void initializeVulkan() {
        VulkanInitializer::initializeVulkan(
            vulkanInstance_, physicalDevice_, vulkanDevice_, surface_,
            graphicsQueue_, presentQueue_, graphicsFamily_, presentFamily_,
            swapchain_, swapchainImages_, swapchainImageViews_, renderPass_,
            pipeline_, pipelineLayout_, swapchainFramebuffers_, commandPool_,
            commandBuffers_, imageAvailableSemaphore_, renderFinishedSemaphore_,
            inFlightFence_, vertexBuffer_, vertexBufferMemory_, indexBuffer_,
            indexBufferMemory_, amouranth_->getSphereVertices(), amouranth_->getSphereIndices(),
            width_, height_);

        // Initialize quad buffers (not used in mode9.cpp but prepared for other modes)
        VulkanInitializer::initializeQuadBuffers(
            vulkanDevice_, physicalDevice_, commandPool_, graphicsQueue_,
            quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_,
            quadIndexBufferMemory_, amouranth_->getQuadVertices(), amouranth_->getQuadIndices());
    }

    // Recreates the swapchain and related resources when the window is resized
    void recreateSwapchain() {
        if (vulkanDevice_ != VK_NULL_HANDLE) {
            VkResult result = vkDeviceWaitIdle(vulkanDevice_);
            if (result != VK_SUCCESS) {
                std::cerr << "vkDeviceWaitIdle failed: " << result << "\n";
            }
        }

        // Clean up existing Vulkan resources
        VulkanInitializer::cleanupVulkan(
            vulkanInstance_, vulkanDevice_, surface_, swapchain_, swapchainImageViews_,
            swapchainFramebuffers_, pipeline_, pipelineLayout_, renderPass_,
            commandPool_, commandBuffers_, imageAvailableSemaphore_, renderFinishedSemaphore_,
            inFlightFence_, vertexBuffer_, vertexBufferMemory_, indexBuffer_, indexBufferMemory_,
            quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_, quadIndexBufferMemory_);

        // Reset Vulkan resources
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
        vulkanDevice_ = VK_NULL_HANDLE;

        // Update window dimensions
        int newWidth, newHeight;
        SDL_GetWindowSize(window_, &newWidth, &newHeight);
        width_ = std::max(1280, newWidth);
        height_ = std::max(720, newHeight);
        if (newWidth < 1280 || newHeight < 720) {
            SDL_SetWindowSize(window_, width_, height_);
        }

        // Reinitialize Vulkan resources
        initializeVulkan();
    }

    // Cleans up all Vulkan and SDL3 resources
    void cleanup() {
        if (vulkanDevice_ != VK_NULL_HANDLE) {
            VkResult result = vkDeviceWaitIdle(vulkanDevice_);
            if (result != VK_SUCCESS) {
                std::cerr << "vkDeviceWaitIdle failed in cleanup: " << result << "\n";
            }
        }

        // Clean up Vulkan resources
        VulkanInitializer::cleanupVulkan(
            vulkanInstance_, vulkanDevice_, surface_, swapchain_, swapchainImageViews_,
            swapchainFramebuffers_, pipeline_, pipelineLayout_, renderPass_,
            commandPool_, commandBuffers_, imageAvailableSemaphore_, renderFinishedSemaphore_,
            inFlightFence_, vertexBuffer_, vertexBufferMemory_, indexBuffer_, indexBufferMemory_,
            quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_, quadIndexBufferMemory_);

        // Clean up SDL3 resources
        sdlInitializer_.cleanup();
        delete amouranth_;
        delete simulator_;
        window_ = nullptr;
        vulkanInstance_ = VK_NULL_HANDLE;
        surface_ = VK_NULL_HANDLE;
    }

    // Renders a single frame using Vulkan
    void render() {
        // Skip rendering if user camera is active (handled by AMOURANTH)
        if (amouranth_->isUserCamActive()) return;

        // Update AMOURANTH state (assumes 60 FPS)
        amouranth_->update(0.016f);

        // Wait for previous frame to complete
        vkWaitForFences(vulkanDevice_, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
        vkResetFences(vulkanDevice_, 1, &inFlightFence_);

        // Acquire next swapchain image
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vulkanDevice_, swapchain_, UINT64_MAX, imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            return;
        }
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to acquire swapchain image: " + std::to_string(result));
        }

        // Reset command buffer for the current frame
        vkResetCommandBuffer(commandBuffers_[imageIndex], 0);

        // Begin command buffer recording
        VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr};
        if (vkBeginCommandBuffer(commandBuffers_[imageIndex], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin command buffer");
        }

        // Begin render pass with a dark gray clear color to distinguish from black screen
        VkClearValue clearColor = {{{0.1f, 0.1f, 0.1f, 1.0f}}};
        VkRenderPassBeginInfo renderPassInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            renderPass_,
            swapchainFramebuffers_[imageIndex],
            {{0, 0}, {(uint32_t)width_, (uint32_t)height_}},
            1,
            &clearColor
        };
        vkCmdBeginRenderPass(commandBuffers_[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Bind the sphere rendering pipeline
        vkCmdBindPipeline(commandBuffers_[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

        // Render using AMOURANTH (calls renderMode9 for mode 9)
        amouranth_->render(imageIndex, vertexBuffer_, commandBuffers_[imageIndex], indexBuffer_, pipelineLayout_);

        // End render pass and command buffer
        vkCmdEndRenderPass(commandBuffers_[imageIndex]);
        if (vkEndCommandBuffer(commandBuffers_[imageIndex]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to end command buffer");
        }

        // Submit command buffer to graphics queue
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        VkSubmitInfo submitInfo = {
            VK_STRUCTURE_TYPE_SUBMIT_INFO,
            nullptr,
            1,
            &imageAvailableSemaphore_,
            waitStages,
            1,
            &commandBuffers_[imageIndex],
            1,
            &renderFinishedSemaphore_
        };
        if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlightFence_) != VK_SUCCESS) {
            throw std::runtime_error("Failed to submit queue");
        }

        // Present the rendered image
        VkPresentInfoKHR presentInfo = {
            VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            nullptr,
            1,
            &renderFinishedSemaphore_,
            1,
            &swapchain_,
            &imageIndex,
            nullptr
        };
        result = vkQueuePresentKHR(presentQueue_, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapchain();
            return;
        }
        if (result != VK_SUCCESS) {
            throw std::runtime_error("Failed to present queue: " + std::to_string(result));
        }
    }
};

#endif // MAIN_HPP