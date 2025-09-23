#ifndef MAIN_HPP
#define MAIN_HPP
// main AMOURANTH RTX engine
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <iostream>
#include <memory>
#include <mutex>
#include <atomic>
#include <omp.h>
#include "SDL3_init.hpp"
#include "Vulkan_init.hpp"
#include "core.hpp"

// Application class managing the Vulkan rendering pipeline and SDL3 window for the Dimensional Navigator.
class Application {
public:
    Application(const char* title = "Dimensional Navigator", int width = 1280, int height = 720)
        : width_(width), height_(height) {
        try {
            // Set OpenMP thread count
            omp_set_num_threads(omp_get_max_threads());

            // Initialize SDL3 and create window, Vulkan instance, and surface
            sdlInitializer_.initialize(title, width, height);
            window_ = sdlInitializer_.getWindow();
            vulkanInstance_ = sdlInitializer_.getVkInstance();
            surface_ = sdlInitializer_.getVkSurface();

            // Create DimensionalNavigator and AMOURANTH with smart pointers
            simulator_ = std::make_unique<DimensionalNavigator>("Dimensional Navigator", width, height);
            amouranth_ = std::make_unique<AMOURANTH>(simulator_.get());

            // Initialize Vulkan resources
            initializeVulkan();

            // Parallelize cache update
            #pragma omp parallel
            {
                #pragma omp single
                {
                    amouranth_->setMode(9);
                    amouranth_->updateCache();
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Initialization failed: " << e.what() << "\n";
            cleanup();
            throw;
        }
    }

    ~Application() {
        cleanup();
    }

    void run() {
        sdlInitializer_.eventLoop(
            [this]() { render(); },
            [this](int w, int h) {
                std::lock_guard<std::mutex> lock(mutex_);
                width_ = std::max(1280, w);
                height_ = std::max(720, h);
                try {
                    recreateSwapchain();
                } catch (const std::exception& e) {
                    std::cerr << "Swapchain recreation failed: " << e.what() << "\n";
                    throw;
                }
            },
            true,
            [this](const SDL_KeyboardEvent& key) {
                std::lock_guard<std::mutex> lock(mutex_);
                amouranth_->handleInput(key);
            }
        );
    }

private:
    std::unique_ptr<DimensionalNavigator> simulator_;
    SDL3Initializer sdlInitializer_;
    SDL_Window* window_;
    VkInstance vulkanInstance_;
    VkDevice vulkanDevice_;
    VkPhysicalDevice physicalDevice_;
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
    uint32_t graphicsFamily_ = UINT32_MAX;
    uint32_t presentFamily_ = UINT32_MAX;
    std::atomic<int> width_;
    std::atomic<int> height_;
    std::unique_ptr<AMOURANTH> amouranth_;
    std::mutex mutex_; // Protects shared resources

    void initializeVulkan() {
        std::lock_guard<std::mutex> lock(mutex_);
        VulkanInitializer::initializeVulkan(
            vulkanInstance_, physicalDevice_, vulkanDevice_, surface_,
            graphicsQueue_, presentQueue_, graphicsFamily_, presentFamily_,
            swapchain_, swapchainImages_, swapchainImageViews_, renderPass_,
            pipeline_, pipelineLayout_, swapchainFramebuffers_, commandPool_,
            commandBuffers_, imageAvailableSemaphore_, renderFinishedSemaphore_,
            inFlightFence_, vertexBuffer_, vertexBufferMemory_, indexBuffer_,
            indexBufferMemory_, amouranth_->getSphereVertices(), amouranth_->getSphereIndices(),
            width_.load(), height_.load());

        VulkanInitializer::initializeQuadBuffers(
            vulkanDevice_, physicalDevice_, commandPool_, graphicsQueue_,
            quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_,
            quadIndexBufferMemory_, amouranth_->getQuadVertices(), amouranth_->getQuadIndices());
    }

    void recreateSwapchain() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (vulkanDevice_ != VK_NULL_HANDLE) {
            VkResult result = vkDeviceWaitIdle(vulkanDevice_);
            if (result != VK_SUCCESS) {
                std::cerr << "vkDeviceWaitIdle failed: " << result << "\n";
            }
        }

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

        int newWidth, newHeight;
        SDL_GetWindowSize(window_, &newWidth, &newHeight);
        width_ = std::max(1280, newWidth);
        height_ = std::max(720, newHeight);
        if (newWidth < 1280 || newHeight < 720) {
            SDL_SetWindowSize(window_, width_.load(), height_.load());
        }

        initializeVulkan();
    }

    void cleanup() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (vulkanDevice_ != VK_NULL_HANDLE) {
            VkResult result = vkDeviceWaitIdle(vulkanDevice_);
            if (result != VK_SUCCESS) {
                std::cerr << "vkDeviceWaitIdle failed in cleanup: " << result << "\n";
            }
        }

        VulkanInitializer::cleanupVulkan(
            vulkanInstance_, vulkanDevice_, surface_, swapchain_, swapchainImageViews_,
            swapchainFramebuffers_, pipeline_, pipelineLayout_, renderPass_,
            commandPool_, commandBuffers_, imageAvailableSemaphore_, renderFinishedSemaphore_,
            inFlightFence_, vertexBuffer_, vertexBufferMemory_, indexBuffer_, indexBufferMemory_,
            quadVertexBuffer_, quadVertexBufferMemory_, quadIndexBuffer_, quadIndexBufferMemory_);

        sdlInitializer_.cleanup();
        window_ = nullptr;
        vulkanInstance_ = VK_NULL_HANDLE;
        surface_ = VK_NULL_HANDLE;
    }

    void render() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (amouranth_->isUserCamActive()) return;

        // Parallelize non-Vulkan updates
        #pragma omp parallel
        {
            #pragma omp single
            {
                amouranth_->update(0.016f);
            }
        }

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

        VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr};
        if (vkBeginCommandBuffer(commandBuffers_[imageIndex], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin command buffer");
        }

        VkClearValue clearColor = {{{0.1f, 0.1f, 0.1f, 1.0f}}};
        VkRenderPassBeginInfo renderPassInfo = {
            VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            nullptr,
            renderPass_,
            swapchainFramebuffers_[imageIndex],
            {{0, 0}, {static_cast<uint32_t>(width_.load()), static_cast<uint32_t>(height_.load())}},
            1,
            &clearColor
        };
        vkCmdBeginRenderPass(commandBuffers_[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffers_[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
        amouranth_->render(imageIndex, vertexBuffer_, commandBuffers_[imageIndex], indexBuffer_, pipelineLayout_);

        vkCmdEndRenderPass(commandBuffers_[imageIndex]);
        if (vkEndCommandBuffer(commandBuffers_[imageIndex]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to end command buffer");
        }

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