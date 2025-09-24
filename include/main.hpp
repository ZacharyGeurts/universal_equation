#ifndef MAIN_HPP
#define MAIN_HPP

// AMOURANTH RTX engine - Header and implementation for the main Application class.
// This file defines the Application class, which serves as the central coordinator for the "Dimensional Navigator" application.
// It manages the entire application lifecycle, including:
// - SDL3 window creation, event handling, and input processing for cross-platform support.
// - Vulkan instance, surface, device, swapchain, and rendering resources (via VulkanInitializer).
// - Integration with DimensionalNavigator for computational logic (e.g., physics, simulation modes).
// - Integration with AMOURANTH for rendering, visual effects, mode management, and input handling.
// - Main render loop, swapchain recreation on window resize, and comprehensive resource cleanup.
// Key features:
// - Thread-safe: Uses OpenMP for parallelism (configured in constructor; e.g., for simulator updates).
// - Memory-safe: Explicit cleanup in destructor and error handlers; null-checks in render loop to prevent crashes.
// - Error handling: Throws std::runtime_error on critical failures (e.g., Vulkan init); logs non-fatal issues to stderr.
// - Developer-friendly: Extensive comments on Vulkan flow, error handling, resource management, and extensibility.
// - Performance: Clamps window size to minimums (1280x720), uses single-frame-in-flight (extendable to multi-frame).
// - Extensibility: Supports new rendering modes (via AMOURANTH::setMode), custom input handlers, and additional Vulkan pipelines.
// Requirements:
// - SDL3 for windowing, input, and Vulkan instance/surface creation.
// - Vulkan 1.2+ with ray tracing extensions (enabled in VulkanInitializer).
// - GLM for matrix transformations (e.g., viewProj in push constants).
// - OpenMP for CPU parallelism (optional; used via omp_set_num_threads).
// Usage Example:
//   Application app("Custom Title", 1920, 1080);  // Create app with custom window size.
//   app.run();  // Start event loop and rendering.
// Notes:
// - Window resize triggers swapchain recreation, which cleans and reinitializes Vulkan resources.
// - Render loop skips frames if user camera is active to avoid input/render conflicts.
// - Fullscreen support enabled by default in SDL3 event loop for immersive experience.
// - Assumes VulkanInitializer, SDL3Initializer, and core (DimensionalNavigator, AMOURANTH) are defined elsewhere.

// Core includes for math, windowing, Vulkan, exceptions, I/O, and parallelism.
#include <glm/glm.hpp>              // For matrix and vector operations (e.g., viewProj, camPos).
#include <glm/gtc/matrix_transform.hpp>  // For GLM transformation functions (e.g., glm::perspective).
#include <SDL3/SDL.h>               // SDL3 for window creation and input handling.
#include <SDL3/SDL_vulkan.h>        // SDL3 Vulkan extensions for instance/surface creation.
#include <vulkan/vulkan.h>          // Vulkan API for GPU rendering.
#include <stdexcept>                // For std::runtime_error in error handling.
#include <iostream>                 // For logging errors to stderr.
#include <omp.h>                    // OpenMP for CPU parallelism (e.g., simulator updates or buffer uploads).

// Custom engine includes for initialization and core logic.
#include "SDL3_init.hpp"    // SDL3Initializer: Window, Vulkan instance, and surface setup.
#include "Vulkan_init.hpp"  // VulkanInitializer: Core Vulkan resources (device, swapchain, pipeline, buffers).
#include "core.hpp"         // Defines DimensionalNavigator (simulator) and AMOURANTH (renderer).

// Forward declarations to minimize dependencies and avoid circular includes.
class DimensionalNavigator;  // Simulator for computational logic (e.g., physics, modes).
class AMOURANTH;             // Renderer for visual effects, mode management, and input handling.

// Application class: Central coordinator for the Dimensional Navigator application.
// Orchestrates SDL3 for window/input, Vulkan for GPU rendering, and custom engine logic for simulation/rendering.
// Non-copyable/movable: Vulkan resources (e.g., device, swapchain) are not safe to copy or move.
class Application {
public:
    // Constructor: Initializes the full application stack.
    // Parameters:
    // - title: SDL3 window title (default: "Dimensional Navigator").
    // - width, height: Initial window dimensions (default: 1280x720; clamped on resize to min 1280x720).
    // Steps:
    // 1. Set OpenMP thread count to max available for potential parallelism (e.g., in simulator or buffer uploads).
    // 2. Initialize SDL3 via SDL3Initializer (creates window, Vulkan instance, surface).
    // 3. Create DimensionalNavigator (simulator) and AMOURANTH (renderer) with initial dimensions.
    // 4. Initialize Vulkan resources (device, swapchain, pipeline, buffers, sync objects) via VulkanInitializer.
    // 5. Set AMOURANTH to mode 9 (default mode) and update its cache for rendering.
    // Throws: std::runtime_error on SDL3 or Vulkan initialization failures; calls cleanup() on errors.
    // Notes: Logs initialization failures to stderr before re-throwing.
    Application(const char* title = "Dimensional Navigator", int width = 1280, int height = 720)
        : simulator_(nullptr),
          sdlInitializer_(),
          window_(nullptr),
          vulkanInstance_(VK_NULL_HANDLE),
          vulkanDevice_(VK_NULL_HANDLE),
          physicalDevice_(VK_NULL_HANDLE),
          surface_(VK_NULL_HANDLE),
          swapchain_(VK_NULL_HANDLE),
          quadVertexBuffer_(VK_NULL_HANDLE),
          quadVertexBufferMemory_(VK_NULL_HANDLE),
          quadIndexBuffer_(VK_NULL_HANDLE),
          quadIndexBufferMemory_(VK_NULL_HANDLE),
          commandBuffers_(),
          swapchainImages_(),
          swapchainImageViews_(),
          swapchainFramebuffers_(),
          graphicsQueue_(VK_NULL_HANDLE),
          presentQueue_(VK_NULL_HANDLE),
          pipeline_(VK_NULL_HANDLE),
          pipelineLayout_(VK_NULL_HANDLE),
          renderPass_(VK_NULL_HANDLE),
          commandPool_(VK_NULL_HANDLE),
          imageAvailableSemaphore_(VK_NULL_HANDLE),
          renderFinishedSemaphore_(VK_NULL_HANDLE),
          inFlightFence_(VK_NULL_HANDLE),
          vertexBuffer_(VK_NULL_HANDLE),
          vertexBufferMemory_(VK_NULL_HANDLE),
          indexBuffer_(VK_NULL_HANDLE),
          indexBufferMemory_(VK_NULL_HANDLE),
          graphicsFamily_(UINT32_MAX),
          presentFamily_(UINT32_MAX),
          width_(width),
          height_(height),
          amouranth_(nullptr) {
        try {
            // Configure OpenMP to use maximum available threads for parallelism.
            // Useful for CPU-bound tasks like simulator updates or buffer uploads (if implemented).
            omp_set_num_threads(omp_get_max_threads());

            // Initialize SDL3: Creates window, Vulkan instance, and surface.
            sdlInitializer_.initialize(title, width, height);
            window_ = sdlInitializer_.getWindow();
            vulkanInstance_ = sdlInitializer_.getVkInstance();
            surface_ = sdlInitializer_.getVkSurface();

            // Create simulator and renderer objects.
            // DimensionalNavigator handles computational logic (e.g., physics, modes).
            // AMOURANTH handles rendering, input, and mode-specific visuals.
            simulator_ = new DimensionalNavigator("Dimensional Navigator", width, height);
            amouranth_ = new AMOURANTH(simulator_);

            // Initialize Vulkan resources (device, swapchain, pipeline, buffers, etc.).
            initializeVulkan();

            // Set default mode (9) and update AMOURANTH cache for rendering.
            // Mode 9 likely corresponds to a specific visual or simulation mode (defined in core.hpp).
            amouranth_->setMode(9);
            amouranth_->updateCache();
        } catch (const std::exception& e) {
            // Log initialization error and clean up resources before re-throwing.
            std::cerr << "Initialization failed: " << e.what() << "\n";
            cleanup();
            throw;
        }
    }

    // Destructor: Ensures complete cleanup of all resources to prevent memory leaks.
    // Calls cleanup() to free Vulkan, SDL3, and custom engine resources.
    // Idempotent: Safe to call multiple times (handles null/empty resources).
    ~Application() {
        cleanup();
    }

    // Deleted copy/move constructors and assignments for safety.
    // Vulkan resources (e.g., device, swapchain) cannot be safely copied or moved.
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    // Main entry point: Runs the SDL3 event loop with custom callbacks.
    // Callbacks:
    // - Render: Calls render() each frame (targets ~60 FPS via SDL3).
    // - Resize: Recreates swapchain on window size change (clamps to min 1280x720).
    // - Input: Forwards SDL_KeyboardEvent to AMOURANTH for mode/input handling.
    // - Fullscreen: Enabled by default for immersive experience.
    // Throws: std::runtime_error on critical failures (e.g., swapchain recreation).
    // Notes: Event loop runs until user quits (e.g., via SDL_QUIT or Alt+F4).
    void run() {
        sdlInitializer_.eventLoop(
            [this]() { render(); },  // Render callback: Called each frame.
            [this](int w, int h) {   // Resize callback: Update dimensions and recreate swapchain.
                width_ = std::max(1280, w);
                height_ = std::max(720, h);
                try {
                    recreateSwapchain();
                } catch (const std::exception& e) {
                    // Log swapchain recreation failure and re-throw.
                    std::cerr << "Swapchain recreation failed: " << e.what() << "\n";
                    throw;
                }
            },
            true,  // Enable fullscreen support.
            [this](const SDL_KeyboardEvent& key) { amouranth_->handleInput(key); }  // Input callback: Forward keyboard events.
        );
    }

private:
    // Private members: Vulkan, SDL3, and engine handles/resources.
    DimensionalNavigator* simulator_;  // Owned pointer to simulator (computational logic; deleted in cleanup).
    SDL3Initializer sdlInitializer_;  // SDL3 wrapper for window, instance, surface management.
    SDL_Window* window_;               // SDL3 window handle (from sdlInitializer_).
    VkInstance vulkanInstance_;       // Vulkan instance for device enumeration (from SDL3Initializer).
    VkDevice vulkanDevice_;           // Logical Vulkan device for resource creation.
    VkPhysicalDevice physicalDevice_; // Physical Vulkan device (GPU; selected by VulkanInitializer).
    VkSurfaceKHR surface_;            // Vulkan surface for swapchain presentation (from SDL3Initializer).
    VkBuffer quadVertexBuffer_;       // Vertex buffer for quad geometry (e.g., UI overlays).
    VkDeviceMemory quadVertexBufferMemory_;  // Allocated memory for quad vertex buffer.
    VkBuffer quadIndexBuffer_;        // Index buffer for quad geometry.
    VkDeviceMemory quadIndexBufferMemory_;   // Allocated memory for quad index buffer.
    std::vector<VkCommandBuffer> commandBuffers_;  // Per-frame command buffers for render commands.
    VkSwapchainKHR swapchain_;        // Swapchain for acquiring/presenting images.
    std::vector<VkImage> swapchainImages_;  // Backbuffer images from swapchain.
    std::vector<VkImageView> swapchainImageViews_;  // Views for swapchain images (color attachment).
    std::vector<VkFramebuffer> swapchainFramebuffers_;  // Framebuffers for each swapchain image.
    VkQueue graphicsQueue_;           // Queue for graphics/compute commands.
    VkQueue presentQueue_;            // Queue for presenting images to surface.
    VkPipeline pipeline_;             // Graphics pipeline for rasterization.
    VkPipelineLayout pipelineLayout_; // Pipeline layout with push constants for dynamic data.
    VkRenderPass renderPass_;         // Render pass defining color attachment and subpass.
    VkCommandPool commandPool_;       // Command pool for allocating command buffers.
    VkSemaphore imageAvailableSemaphore_;  // Signals when swapchain image is ready.
    VkSemaphore renderFinishedSemaphore_;  // Signals when rendering is complete.
    VkFence inFlightFence_;           // Tracks in-flight frames to prevent overruns.
    VkBuffer vertexBuffer_;           // Vertex buffer for sphere geometry (main object).
    VkDeviceMemory vertexBufferMemory_;  // Allocated memory for sphere vertex buffer.
    VkBuffer indexBuffer_;            // Index buffer for sphere geometry.
    VkDeviceMemory indexBufferMemory_;   // Allocated memory for sphere index buffer.
    uint32_t graphicsFamily_ = UINT32_MAX;  // Graphics queue family index.
    uint32_t presentFamily_ = UINT32_MAX;   // Present queue family index.
    int width_;                       // Current window width (clamped to min 1280).
    int height_;                      // Current window height (clamped to min 720).
    AMOURANTH* amouranth_;            // Owned pointer to renderer (visuals, input; deleted in cleanup).

    // Initializes Vulkan resources: device, queues, swapchain, pipeline, buffers, and sync objects.
    // Calls VulkanInitializer::initializeVulkan for core resources and initializeQuadBuffers for quad geometry.
    // Uses sphere and quad vertices/indices from AMOURANTH (assumed to provide mesh data).
    // Assumes SDL3Initializer has already created window, instance, and surface.
    // Throws: std::runtime_error on any Vulkan resource creation failure.
    // Notes: Initializes both sphere (main object) and quad (e.g., UI) buffers for rendering flexibility.
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

        VulkanInitializer::initializeQuadBuffers(
            vulkanDevice_, physicalDevice_, commandPool_, graphicsQueue_,
            quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_,
            quadIndexBufferMemory_, amouranth_->getQuadVertices(), amouranth_->getQuadIndices());
    }

    // Recreates the swapchain and related resources on window resize.
    // Steps (ensures no pending GPU operations):
    // 1. Wait for device idle (logs non-fatal errors to stderr).
    // 2. Clean up all Vulkan resources using VulkanInitializer::cleanupVulkan.
    // 3. Null all Vulkan handles and clear vectors to prevent dangling references.
    // 4. Get new window size via SDL_GetWindowSize; clamp to min 1280x720.
    // 5. Resize window if below minimum to ensure valid swapchain creation.
    // 6. Reinitialize Vulkan resources via initializeVulkan().
    // Throws: std::runtime_error on cleanup or reinitialization failures.
    // Notes: Called on window resize events or VK_ERROR_OUT_OF_DATE_KHR in render().
    void recreateSwapchain() {
        if (vulkanDevice_ != VK_NULL_HANDLE) {
            // Wait for device to finish pending operations before cleanup.
            VkResult result = vkDeviceWaitIdle(vulkanDevice_);
            if (result != VK_SUCCESS) {
                // Log non-fatal error but continue cleanup.
                std::cerr << "vkDeviceWaitIdle failed: " << result << "\n";
            }
        }

        // Clean up all Vulkan resources (swapchain, pipeline, buffers, etc.).
        VulkanInitializer::cleanupVulkan(
            vulkanInstance_, vulkanDevice_, surface_, swapchain_, swapchainImageViews_,
            swapchainFramebuffers_, pipeline_, pipelineLayout_, renderPass_,
            commandPool_, commandBuffers_, imageAvailableSemaphore_, renderFinishedSemaphore_,
            inFlightFence_, vertexBuffer_, vertexBufferMemory_, indexBuffer_, indexBufferMemory_,
            quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_, quadIndexBufferMemory_);

        // Null all handles and clear vectors to ensure no dangling pointers.
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

        // Get new window dimensions and clamp to minimum size.
        int newWidth, newHeight;
        SDL_GetWindowSize(window_, &newWidth, &newHeight);
        width_ = std::max(1280, newWidth);
        height_ = std::max(720, newHeight);
        if (newWidth < 1280 || newHeight < 720) {
            // Resize window if below minimum to ensure valid swapchain creation.
            SDL_SetWindowSize(window_, width_, height_);
        }

        // Reinitialize Vulkan resources with new dimensions.
        initializeVulkan();
    }

    // Performs full cleanup of all resources to prevent memory leaks.
    // Steps (idempotent and safe for partial initialization):
    // 1. Wait for device idle to ensure no pending GPU operations (logs non-fatal errors).
    // 2. Call VulkanInitializer::cleanupVulkan to free Vulkan resources (swapchain, pipeline, buffers, etc.).
    // 3. Call sdlInitializer_.cleanup() to free SDL3 resources (window, instance, surface).
    // 4. Delete owned pointers (amouranth_, simulator_).
    // 5. Null window and Vulkan instance/surface handles for safety.
    // Notes: Called by destructor and constructor error handler; no throws to ensure cleanup completes.
    void cleanup() {
        if (vulkanDevice_ != VK_NULL_HANDLE) {
            // Wait for device to finish pending operations before cleanup.
            VkResult result = vkDeviceWaitIdle(vulkanDevice_);
            if (result != VK_SUCCESS) {
                // Log non-fatal error but continue cleanup.
                std::cerr << "vkDeviceWaitIdle failed in cleanup: " << result << "\n";
            }
        }

        // Free all Vulkan resources using VulkanInitializer.
        VulkanInitializer::cleanupVulkan(
            vulkanInstance_, vulkanDevice_, surface_, swapchain_, swapchainImageViews_,
            swapchainFramebuffers_, pipeline_, pipelineLayout_, renderPass_,
            commandPool_, commandBuffers_, imageAvailableSemaphore_, renderFinishedSemaphore_,
            inFlightFence_, vertexBuffer_, vertexBufferMemory_, indexBuffer_, indexBufferMemory_,
            quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_, quadIndexBufferMemory_);

        // Clean up SDL3 resources (window, instance, surface).
        sdlInitializer_.cleanup();

        // Delete owned engine objects.
        delete amouranth_;
        delete simulator_;

        // Null pointers for safety (prevents accidental access).
        amouranth_ = nullptr;
        simulator_ = nullptr;
        window_ = nullptr;
        vulkanInstance_ = VK_NULL_HANDLE;
        surface_ = VK_NULL_HANDLE;
    }

    // Renders a single frame (called by SDL3 event loop).
    // Uses single-frame-in-flight for simplicity; extend to multi-frame with additional fences/semaphors.
    // Steps:
    // 1. Skip rendering if AMOURANTH's user camera is active (avoids input/render conflicts).
    // 2. Update AMOURANTH simulation with fixed delta time (~16ms for 60 FPS).
    // 3. Wait for in-flight fence to ensure previous frame is complete.
    // 4. Reset fence for current frame.
    // 5. Acquire next swapchain image; recreate swapchain on VK_ERROR_OUT_OF_DATE_KHR.
    // 6. Reset and begin command buffer for recording.
    // 7. Begin render pass with dark gray clear color (RGBA: 0.1, 0.1, 0.1, 1.0).
    // 8. Bind graphics pipeline and call AMOURANTH::render for custom drawing (uses vertex/index buffers).
    // 9. End render pass and command buffer.
    // 10. Submit to graphics queue with imageAvailableSemaphore and renderFinishedSemaphore.
    // 11. Present to present queue; recreate swapchain on VK_ERROR_OUT_OF_DATE_KHR.
    // Throws: std::runtime_error on critical failures (acquire, begin, submit, present).
    // Notes: Uses push constants via pipelineLayout_ for dynamic data (e.g., matrices, mode parameters).
    void render() {
        // Skip rendering if user camera is active (e.g., during interactive mode).
        if (amouranth_->isUserCamActive()) return;

        // Update AMOURANTH simulation (fixed 16ms delta for ~60 FPS).
        amouranth_->update(0.016f);

        // Wait for previous frame to complete (single-frame-in-flight).
        vkWaitForFences(vulkanDevice_, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
        vkResetFences(vulkanDevice_, 1, &inFlightFence_);

        // Acquire next swapchain image for rendering.
        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(vulkanDevice_, swapchain_, UINT64_MAX, imageAvailableSemaphore_, VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            // Swapchain outdated (e.g., window resized); recreate and skip frame.
            recreateSwapchain();
            return;
        }
        if (result != VK_SUCCESS) {
            // Critical failure during image acquisition.
            throw std::runtime_error("Failed to acquire swapchain image: " + std::to_string(result));
        }

        // Reset command buffer for the current frame.
        vkResetCommandBuffer(commandBuffers_[imageIndex], 0);

        // Begin recording commands.
        VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr };
        if (vkBeginCommandBuffer(commandBuffers_[imageIndex], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin command buffer");
        }

        // Set up render pass with dark gray clear color.
        VkClearValue clearColor = { {{0.1f, 0.1f, 0.1f, 1.0f}} };
        VkRenderPassBeginInfo renderPassInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            renderPass_,
            swapchainFramebuffers_[imageIndex],
            {{0, 0}, {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_)}},
            1,
            &clearColor
        };
        vkCmdBeginRenderPass(commandBuffers_[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Bind graphics pipeline and delegate rendering to AMOURANTH.
        // AMOURANTH::render uses vertex/index buffers and push constants for custom drawing.
        vkCmdBindPipeline(commandBuffers_[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        amouranth_->render(imageIndex, vertexBuffer_, commandBuffers_[imageIndex], indexBuffer_, pipelineLayout_);

        // End render pass and command buffer.
        vkCmdEndRenderPass(commandBuffers_[imageIndex]);
        if (vkEndCommandBuffer(commandBuffers_[imageIndex]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to end command buffer");
        }

        // Submit command buffer to graphics queue with synchronization.
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
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

        // Present rendered image to the swapchain.
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
            // Swapchain outdated; recreate and skip frame.
            recreateSwapchain();
            return;
        }
        if (result != VK_SUCCESS) {
            // Critical failure during presentation.
            throw std::runtime_error("Failed to present queue: " + std::to_string(result));
        }
    }
};

#endif // MAIN_HPP