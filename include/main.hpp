#ifndef MAIN_HPP
#define MAIN_HPP

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <vector>
#include <iostream>
#include <map>
#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"
#include "SDL3_ttf/SDL_ttf.h"
#include "vulkan/vulkan.h"
#include "SDL3_init.hpp"
#include "Vulkan_init.hpp"
#include "universal_equation.hpp"

// Structure to hold dimension data
struct DimensionData {
    int dimension;
    double observable;
    double potential;
    double darkMatter;
    double darkEnergy;
};

struct PushConstants {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec3 baseColor; // Added for pink baseline color
    float value;
    float dimension;
    float wavePhase;
    float cycleProgress;
    float darkMatter;
    float darkEnergy;
};

class DimensionalNavigator {
public:
    DimensionalNavigator(const char* title = "Dimensional Navigator",
                        int width = 1280, int height = 720,
                        const char* fontPath = "arial.ttf", int fontSize = 24)
        : window_(nullptr), font_(nullptr), vulkanInstance_(VK_NULL_HANDLE), vulkanDevice_(VK_NULL_HANDLE),
          physicalDevice_(VK_NULL_HANDLE), surface_(VK_NULL_HANDLE), swapchain_(VK_NULL_HANDLE),
          mode_(1), wavePhase_(0.0f), waveSpeed_(0.1f), width_(width), height_(height), zoomLevel_(1.0f) {
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

    void updateZoom(bool zoomIn) {
        if (zoomIn) {
            zoomLevel_ = std::max(0.1f, zoomLevel_ * 0.9f);
        } else {
            zoomLevel_ = std::min(10.0f, zoomLevel_ * 1.1f);
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
                case SDLK_5:
                    mode_ = 5;
                    break;
                case SDLK_A:
                    updateZoom(true);
                    break;
                case SDLK_Z:
                    updateZoom(false);
                    break;
            }
        }
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
    float zoomLevel_;
    static constexpr int kMaxRenderedDimensions = 4;

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
        ue_ = UniversalEquation(9, 1, 1.0, 0.5, 0.5, 0.5, 1.5, 2.0, 0.27, 0.68, 5.0, 0.2, true); // Enable debug
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

void renderMode1(uint32_t imageIndex) {
    VkBuffer vertexBuffers[] = {quadVertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffers_[imageIndex], quadIndexBuffer_, 0, VK_INDEX_TYPE_UINT32);

    float zoomFactor = zoomLevel_;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width_ / height_, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 10.0f * zoomFactor),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    float cycleProgress = std::fmod(wavePhase_ / (2.0f * kMaxRenderedDimensions), 1.0f);

    if (cache_.size() < kMaxRenderedDimensions) {
        std::cerr << "Warning: Cache size " << cache_.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    for (size_t i = 0; i < kMaxRenderedDimensions; ++i) {
        if (i > 0) continue;
        if (cache_[i].dimension != 1) {
            std::cerr << "Warning: Invalid cache for dimension 1\n";
            continue;
        }

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(100.0f * zoomFactor, 100.0f * zoomFactor, 0.01f));
        model = glm::rotate(model, wavePhase_ * 0.1f, glm::vec3(0.0f, 1.0f, 0.0f));

        float dimValue = 1.0f;
        float value = static_cast<float>(cache_[i].observable * (0.8f + 0.2f * cosf(wavePhase_)));
        if (value < 0.01f) value = 0.5f;
        glm::vec3 baseColor = glm::vec3(1.0f, 1.0f, 1.0f); // White base for mode 1
        PushConstants pushConstants = {
            model,
            view,
            proj,
            baseColor,
            value,
            dimValue,
            wavePhase_,
            cycleProgress,
            static_cast<float>(cache_[i].darkMatter),
            static_cast<float>(cache_[i].darkEnergy)
        };
        vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(quadIndices_.size()), 1, 0, 0, 0);
    }

    ue_.setCurrentDimension(1);
    auto pairs = ue_.getInteractions();
    if (pairs.empty()) {
        std::cerr << "Warning: No interactions for dimension 1\n";
        glm::mat4 interactionModel = glm::mat4(1.0f);
        interactionModel = glm::translate(interactionModel, glm::vec3(0.0f, 0.0f, 0.0f));
        interactionModel = glm::scale(interactionModel, glm::vec3(50.0f * zoomFactor, 50.0f * zoomFactor, 0.01f));
        glm::vec3 baseColor = glm::vec3(1.0f, 1.0f, 1.0f); // White base for interactions
        PushConstants pushConstants = {
            interactionModel,
            view,
            proj,
            baseColor,
            0.5f,
            1.0f,
            wavePhase_,
            cycleProgress,
            0.5f,
            0.5f
        };
        vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(quadIndices_.size()), 1, 0, 0, 0);
    } else {
        for (const auto& pair : pairs) {
            if (pair.dimension > 1) continue;

            float interactionStrength = static_cast<float>(
                computeInteraction(pair.dimension, pair.distance) *
                std::exp(-ue_.getAlpha() * pair.distance) *
                computePermeation(pair.dimension) *
                pair.darkMatterDensity);
            if (interactionStrength < 0.01f) interactionStrength = 0.5f;

            float offset = static_cast<float>(pair.distance) * 0.5f * (1.0f + static_cast<float>(pair.darkMatterDensity) * 0.2f);
            glm::vec3 offsetPos = glm::vec3(0.0f, offset * sinf(wavePhase_ + pair.dimension), offset * cosf(wavePhase_ + pair.dimension));
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), offsetPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(50.0f * zoomFactor, 50.0f * zoomFactor, 0.01f));

            glm::vec3 baseColor = glm::vec3(1.0f, 1.0f, 1.0f); // White base for interactions
            PushConstants pushConstants = {
                interactionModel,
                view,
                proj,
                baseColor,
                interactionStrength * (0.7f + 0.3f * cosf(wavePhase_)),
                1.0f,
                wavePhase_,
                cycleProgress,
                static_cast<float>(pair.darkMatterDensity),
                static_cast<float>(computeDarkEnergy(pair.distance))
            };
            vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_,
                              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                              0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(quadIndices_.size()), 1, 0, 0, 0);
        }
    }
}

void renderMode2(uint32_t imageIndex) {
    VkBuffer vertexBuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffers_[imageIndex], indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

    float zoomFactor = zoomLevel_;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width_ / height_, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 10.0f * zoomFactor),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    constexpr float baseRadius = 0.5f;
    float cycleProgress = std::fmod(wavePhase_ / (2.0f * kMaxRenderedDimensions), 1.0f);

    if (cache_.size() < kMaxRenderedDimensions) {
        std::cerr << "Warning: Cache size " << cache_.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    for (size_t i = 0; i < kMaxRenderedDimensions; ++i) {
        if (cache_[i].dimension != static_cast<int>(i + 1)) {
            std::cerr << "Warning: Invalid cache for dimension " << i + 1 << "\n";
            continue;
        }

        float emphasis = (i == 1) ? 2.0f : 0.5f;
        float radius = baseRadius * emphasis * (1.0f + static_cast<float>(cache_[i].observable) * 0.2f) *
                       (1.0f + 0.1f * sinf(wavePhase_ + i) * (1.0f + static_cast<float>(cache_[i].darkMatter) * 0.5f));
        radius *= zoomFactor;
        if (radius < 0.01f) radius = 0.1f * zoomFactor;

        glm::mat4 model = glm::mat4(1.0f);
        float angle = wavePhase_ + (i + 1) * 2.0f * glm::pi<float>() / kMaxRenderedDimensions;
        float spacing = 1.5f * (1.0f + static_cast<float>(cache_[i].darkEnergy) * 0.5f);
        float x = cosf(angle) * spacing;
        float y = sinf(angle) * spacing;
        float z = -static_cast<float>(i) * 0.5f;
        if (i == 1) {
            model = glm::translate(model, glm::vec3(x * 1.5f * zoomFactor, y * 1.5f * zoomFactor, z));
            model = glm::scale(model, glm::vec3(radius));
        } else if (i == 0) {
            model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
            model = glm::scale(model, glm::vec3(radius));
        } else {
            model = glm::translate(model, glm::vec3(x * zoomFactor, y * zoomFactor, z));
            model = glm::scale(model, glm::vec3(radius));
        }

        float dimValue = static_cast<float>(i + 1);
        float value = static_cast<float>(cache_[i].observable);
        if (value < 0.01f) value = 0.5f;
        glm::vec3 baseColor = glm::vec3(1.0f, 1.0f, 1.0f); // White base for mode 2
        PushConstants pushConstants = {
            model,
            view,
            proj,
            baseColor,
            value,
            dimValue,
            wavePhase_,
            cycleProgress,
            static_cast<float>(cache_[i].darkMatter),
            static_cast<float>(cache_[i].darkEnergy)
        };
        vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);
        std::cerr << "Mode2[D=" << i + 1 << "]: radius=" << radius << ", value=" << value
                  << ", pos=(" << x << ", " << y << ", " << z << ")\n";
    }

    ue_.setCurrentDimension(2);
    auto pairs = ue_.getInteractions();
    if (pairs.empty()) {
        std::cerr << "Warning: No interactions for dimension 2\n";
        glm::mat4 interactionModel = glm::mat4(1.0f);
        interactionModel = glm::translate(interactionModel, glm::vec3(0.0f, 0.0f, 0.0f));
        interactionModel = glm::scale(interactionModel, glm::vec3(0.5f * zoomFactor));
        glm::vec3 baseColor = glm::vec3(1.0f, 1.0f, 1.0f); // White base for interactions
        PushConstants pushConstants = {
            interactionModel,
            view,
            proj,
            baseColor,
            0.5f,
            2.0f,
            wavePhase_,
            cycleProgress,
            0.5f,
            0.5f
        };
        vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);
    } else {
        for (const auto& pair : pairs) {
            if (pair.dimension != 2) continue;

            float interactionStrength = static_cast<float>(
                computeInteraction(pair.dimension, pair.distance) *
                std::exp(-ue_.getAlpha() * pair.distance) *
                computePermeation(pair.dimension) *
                pair.darkMatterDensity);
            if (interactionStrength < 0.01f) interactionStrength = 0.5f;

            float offset = static_cast<float>(pair.distance) * 0.5f * (1.0f + static_cast<float>(pair.darkMatterDensity) * 0.2f);
            glm::vec3 offsetPos = glm::vec3(offset * sinf(wavePhase_ + pair.dimension) * zoomFactor,
                                            offset * cosf(wavePhase_ + pair.dimension) * zoomFactor,
                                            0.0f);
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), offsetPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(0.5f * zoomFactor));

            glm::vec3 baseColor = glm::vec3(1.0f, 1.0f, 1.0f); // White base for interactions
            PushConstants pushConstants = {
                interactionModel,
                view,
                proj,
                baseColor,
                interactionStrength * (0.7f + 0.3f * cosf(wavePhase_)),
                2.0f,
                wavePhase_,
                cycleProgress,
                static_cast<float>(pair.darkMatterDensity),
                static_cast<float>(computeDarkEnergy(pair.distance))
            };
            vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_,
                              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                              0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);
        }
    }
}

void renderMode3(uint32_t imageIndex) {
    VkBuffer vertexBuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffers_[imageIndex], indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

    float zoomFactor = zoomLevel_;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width_ / height_, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(
        glm::vec3(10.0f * zoomFactor, 10.0f * zoomFactor, 10.0f * zoomFactor),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    float cycleProgress = std::fmod(wavePhase_ / (2.0f * kMaxRenderedDimensions), 1.0f);

    if (cache_.size() < kMaxRenderedDimensions) {
        std::cerr << "Warning: Cache size " << cache_.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    // Render the main sphere for dimension 3
    for (size_t i = 0; i < kMaxRenderedDimensions; ++i) {
        if (i != 2) continue; // Only render dimension 3
        if (cache_[i].dimension != 3) {
            std::cerr << "Warning: Invalid cache for dimension 3\n";
            continue;
        }

        // Dynamic scaling based on UniversalEquation
        float observableScale = 1.0f + static_cast<float>(cache_[i].observable) * 0.3f; // e.g., 3.226497 from log
        float darkMatterScale = 1.0f + static_cast<float>(cache_[i].darkMatter) * 0.5f; // e.g., 0.341967
        float darkEnergyScale = 1.0f + static_cast<float>(cache_[i].darkEnergy) * 0.4f; // e.g., 0.909233
        float radius = 0.7f * observableScale * darkMatterScale * darkEnergyScale * (1.0f + 0.2f * sinf(wavePhase_));
        radius *= zoomFactor;
        if (radius < 0.1f) radius = 0.1f * zoomFactor;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f)); // Centered for main sphere
        model = glm::scale(model, glm::vec3(radius));
        model = glm::rotate(model, wavePhase_ * 0.1f, glm::vec3(1.0f, 1.0f, 1.0f));

        float dimValue = 3.0f;
        float value = static_cast<float>(cache_[i].observable) * (0.8f + 0.2f * cosf(wavePhase_));
        if (value < 0.01f) value = 0.5f;
        glm::vec3 baseColor = glm::vec3(1.0f, 1.0f, 1.0f); // White base for mode 3

        PushConstants pushConstants = {
            model,
            view,
            proj,
            baseColor,
            value,
            dimValue,
            wavePhase_,
            cycleProgress,
            static_cast<float>(cache_[i].darkMatter),
            static_cast<float>(cache_[i].darkEnergy)
        };
        vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);

        std::cerr << "Mode3[D=3]: radius=" << radius << ", value=" << value
                  << ", pos=(0, 0, 0), color=(" << baseColor.x << ", " << baseColor.y << ", " << baseColor.z << ")\n";
    }

    // Render interactions for dimension 3
    ue_.setCurrentDimension(3);
    auto pairs = ue_.getInteractions();
    if (pairs.empty()) {
        std::cerr << "Warning: No interactions for dimension 3\n";
        glm::mat4 interactionModel = glm::mat4(1.0f);
        interactionModel = glm::translate(interactionModel, glm::vec3(0.0f, 0.0f, 0.0f));
        interactionModel = glm::scale(interactionModel, glm::vec3(0.3f * zoomFactor));
        glm::vec3 baseColor = glm::vec3(1.0f, 1.0f, 1.0f); // White base for interactions
        PushConstants pushConstants = {
            interactionModel,
            view,
            proj,
            baseColor,
            0.5f,
            3.0f,
            wavePhase_,
            cycleProgress,
            0.5f,
            0.5f
        };
        vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);
    } else {
        for (const auto& pair : pairs) {
            // Include interactions with dimensions 2, 3, and 4 (from log: D=2, D=3, D=4, D=1)
            if (pair.dimension != 1 && pair.dimension != 2 && pair.dimension != 3 && pair.dimension != 4) continue;

            float interactionStrength = static_cast<float>(
                computeInteraction(pair.dimension, pair.distance) *
                std::exp(-ue_.getAlpha() * pair.distance) *
                computePermeation(pair.dimension) *
                pair.darkMatterDensity);
            if (interactionStrength < 0.01f) interactionStrength = 0.5f;

            float orbitRadius = 1.5f + static_cast<float>(pair.distance) * 0.5f * (1.0f + static_cast<float>(pair.darkMatterDensity) * 0.2f);
            float angle = wavePhase_ + pair.dimension * 2.0f;
            glm::vec3 orbitPos = glm::vec3(
                cosf(angle) * orbitRadius * zoomFactor,
                sinf(angle) * orbitRadius * zoomFactor,
                cosf(angle + pair.dimension) * orbitRadius * 0.5f * zoomFactor
            );
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), orbitPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(0.3f * zoomFactor));

            glm::vec3 baseColor = glm::vec3(1.0f, 1.0f, 1.0f); // White base for interactions
            PushConstants pushConstants = {
                interactionModel,
                view,
                proj,
                baseColor,
                interactionStrength * (0.7f + 0.3f * cosf(wavePhase_)),
                3.0f,
                wavePhase_,
                cycleProgress,
                static_cast<float>(pair.darkMatterDensity),
                static_cast<float>(computeDarkEnergy(pair.distance))
            };
            vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_,
                              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                              0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);

            std::cerr << "Mode3 Interaction[D=" << pair.dimension << "]: strength=" << interactionStrength
                      << ", orbitRadius=" << orbitRadius << ", pos=("
                      << orbitPos.x << ", " << orbitPos.y << ", " << orbitPos.z << ")\n";
        }
    }
}

void renderMode4(uint32_t imageIndex) {
    VkBuffer vertexBuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffers_[imageIndex], indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

    float zoomFactor = zoomLevel_;
    glm::mat4 proj = glm::perspective(glm::radians(45.0f), (float)width_ / height_, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(
        glm::vec3(10.0f * zoomFactor, 10.0f * zoomFactor, 10.0f * zoomFactor),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    float cycleProgress = std::fmod(wavePhase_ / (2.0f * kMaxRenderedDimensions), 1.0f);

    if (cache_.size() < kMaxRenderedDimensions) {
        std::cerr << "Warning: Cache size " << cache_.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    for (size_t i = 0; i < kMaxRenderedDimensions; ++i) {
        if (i != 3) continue;
        if (cache_[i].dimension != 4) {
            std::cerr << "Warning: Invalid cache for dimension 4\n";
            continue;
        }

        float alpha = 2.0f;
        float omega = 0.3f;
        float observableRadius = 1.0f * (1.0f + static_cast<float>(cache_[i].observable) * 0.25f);
        float potentialRadius = 1.0f * (1.0f + static_cast<float>(cache_[i].potential) * 0.25f);
        float timeModulation = sinf(wavePhase_ * 1.5f + i) * (1.0f + static_cast<float>(cache_[i].darkMatter) * 0.6f);
        float geometryScale = 0.5f * (observableRadius + potentialRadius);
        float scaledGeometry = geometryScale * (alpha + omega * timeModulation);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
        model = glm::scale(model, glm::vec3(scaledGeometry * zoomFactor * 0.8f, scaledGeometry * zoomFactor * 1.2f, scaledGeometry * zoomFactor));
        model = glm::rotate(model, wavePhase_ * 0.15f, glm::vec3(0.7f, 1.0f, 0.7f));
        model = glm::scale(model, glm::vec3(1.0f + 0.3f * sinf(wavePhase_ * 0.7f)));

        float fluctuationRatio = cache_[i].potential > 0 ? static_cast<float>(cache_[i].observable / cache_[i].potential) : 1.0f;
        float dimValue = 4.0f;
        glm::vec3 baseColor = glm::vec3(
            0.4f + 0.2f * sinf(wavePhase_),
            0.3f + 0.2f * cosf(wavePhase_ * 0.8f),
            0.6f + 0.3f * sinf(wavePhase_ * 1.2f)
        );

        PushConstants pushConstants = {
            model,
            view,
            proj,
            baseColor,
            fluctuationRatio * (0.8f + 0.4f * cosf(wavePhase_ * 1.2f)),
            dimValue,
            wavePhase_,
            cycleProgress,
            static_cast<float>(cache_[i].darkMatter),
            static_cast<float>(cache_[i].darkEnergy)
        };
        vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);

        std::cerr << "Mode4[D=4]: scaledGeometry=" << scaledGeometry << ", fluctuationRatio=" << fluctuationRatio
                  << ", pos=(0, 0, 0), color=(" << baseColor.x << ", " << baseColor.y << ", " << baseColor.z << ")\n";
    }

    ue_.setCurrentDimension(4);
    auto pairs = ue_.getInteractions();
    if (pairs.empty()) {
        std::cerr << "Warning: No interactions for dimension 4\n";
        glm::mat4 interactionModel = glm::mat4(1.0f);
        interactionModel = glm::translate(interactionModel, glm::vec3(0.0f, 0.0f, 0.0f));
        interactionModel = glm::scale(interactionModel, glm::vec3(0.3f * zoomFactor));
        glm::vec3 baseColor = glm::vec3(
            0.4f + 0.2f * sinf(wavePhase_),
            0.3f + 0.2f * cosf(wavePhase_ * 0.8f),
            0.6f + 0.3f * sinf(wavePhase_ * 1.2f)
        );
        PushConstants pushConstants = {
            interactionModel,
            view,
            proj,
            baseColor,
            0.5f,
            4.0f,
            wavePhase_,
            cycleProgress,
            0.5f,
            0.5f
        };
        vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);
    } else {
        for (const auto& pair : pairs) {
            if (pair.dimension != 1 && pair.dimension != 2 && pair.dimension != 3 && pair.dimension != 4 && pair.dimension != 5) continue;

            // Simplified interaction strength using public data
            float interactionStrength = static_cast<float>(
                (1.0f / (1.0f + pair.distance)) * pair.darkMatterDensity * (0.5f + 0.5f * cosf(wavePhase_ + pair.dimension))
            );
            if (interactionStrength < 0.01f) interactionStrength = 0.5f;

            float orbitRadius = 1.5f + static_cast<float>(pair.distance) * 0.5f * (1.0f + static_cast<float>(pair.darkMatterDensity) * 0.3f);
            float angle = wavePhase_ * 1.2f + pair.dimension * 1.5f;
            glm::vec3 orbitPos = glm::vec3(
                cosf(angle) * orbitRadius * zoomFactor,
                sinf(angle) * orbitRadius * zoomFactor,
                (angle * 0.2f + pair.dimension * 0.5f) * zoomFactor
            );
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), orbitPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(0.25f * zoomFactor * (1.0f + 0.2f * sinf(wavePhase_))));

            glm::vec3 baseColor = glm::vec3(
                0.4f + 0.2f * sinf(wavePhase_ + pair.dimension),
                0.3f + 0.2f * cosf(wavePhase_ * 0.8f + pair.dimension),
                0.6f + 0.3f * sinf(wavePhase_ * 1.2f + pair.dimension)
            );
            PushConstants pushConstants = {
                interactionModel,
                view,
                proj,
                baseColor,
                interactionStrength * (0.8f + 0.4f * cosf(wavePhase_ * 1.2f)),
                4.0f,
                wavePhase_,
                cycleProgress,
                static_cast<float>(pair.darkMatterDensity),
                0.5f // Default value since computeDarkEnergy is inaccessible
            };
            vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_,
                              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                              0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);

            std::cerr << "Mode4 Interaction[D=" << pair.dimension << "]: strength=" << interactionStrength
                      << ", orbitRadius=" << orbitRadius << ", pos=("
                      << orbitPos.x << ", " << orbitPos.y << ", " << orbitPos.z << ")\n";
        }
    }
}

void renderMode5(uint32_t imageIndex) {
    VkBuffer vertexBuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffers_[imageIndex], 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffers_[imageIndex], indexBuffer_, 0, VK_INDEX_TYPE_UINT32);

    float zoomFactor = zoomLevel_;
    glm::mat4 proj = glm::perspective(glm::radians(60.0f), (float)width_ / height_, 0.1f, 1000.0f);
    glm::mat4 view = glm::lookAt(
        glm::vec3(15.0f * zoomFactor, 15.0f * zoomFactor, 15.0f * zoomFactor),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    float cycleProgress = std::fmod(wavePhase_ / (2.0f * kMaxRenderedDimensions), 1.0f);
    float heavenGlow = 0.8f + 0.2f * sinf(wavePhase_ * 0.5f); // Pulsating glow effect
    glm::vec3 pinkBaseColor = glm::vec3(1.0f, 0.41f, 0.71f); // Pink baseline color

    if (cache_.size() < kMaxRenderedDimensions) {
        std::cerr << "Warning: Cache size " << cache_.size() << " < " << kMaxRenderedDimensions << "\n";
        return;
    }

    // Render all dimensions as interconnected spheres
    for (size_t i = 0; i < kMaxRenderedDimensions; ++i) {
        if (cache_[i].dimension != static_cast<int>(i + 1)) {
            std::cerr << "Warning: Invalid cache for dimension " << i + 1 << "\n";
            continue;
        }

        // Use UniversalEquation math for scaling
        float observableScale = 1.0f + static_cast<float>(cache_[i].observable) * 0.3f;
        float darkMatterScale = 1.0f + static_cast<float>(cache_[i].darkMatter) * 0.5f;
        float darkEnergyScale = 1.0f + static_cast<float>(cache_[i].darkEnergy) * 0.4f;
        float radius = 0.7f * observableScale * darkMatterScale * darkEnergyScale * (1.0f + 0.2f * sinf(wavePhase_ + i));
        radius *= zoomFactor;
        if (radius < 0.1f) radius = 0.1f * zoomFactor;

        // Tetrahedral arrangement for celestial layout
        float angle = wavePhase_ + (i + 1) * 2.0f * glm::pi<float>() / kMaxRenderedDimensions;
        float spacing = 2.0f * (1.0f + static_cast<float>(cache_[i].darkEnergy) * 0.5f);
        glm::vec3 pos;
        switch (i) {
            case 0: pos = glm::vec3(0.0f, 0.0f, 0.0f); break; // Center
            case 1: pos = glm::vec3(spacing * cosf(angle), spacing * sinf(angle), 0.0f); break;
            case 2: pos = glm::vec3(spacing * cosf(angle + 2.0f * glm::pi<float>() / 3.0f), 
                                    spacing * sinf(angle + 2.0f * glm::pi<float>() / 3.0f), spacing); break;
            case 3: pos = glm::vec3(spacing * cosf(angle + 4.0f * glm::pi<float>() / 3.0f), 
                                    spacing * sinf(angle + 4.0f * glm::pi<float>() / 3.0f), -spacing); break;
        }
        pos *= zoomFactor;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, pos);
        model = glm::scale(model, glm::vec3(radius));
        model = glm::rotate(model, wavePhase_ * 0.2f, glm::vec3(0.5f, 0.5f, 0.5f));

        float dimValue = static_cast<float>(i + 1);
        float value = static_cast<float>(cache_[i].observable) * heavenGlow;
        if (value < 0.01f) value = 0.5f;

        // Modulate pink color with dark matter and dark energy
        glm::vec3 modulatedColor = pinkBaseColor * (0.8f + 0.2f * static_cast<float>(cache_[i].darkMatter) * heavenGlow);
        modulatedColor = glm::clamp(modulatedColor, 0.0f, 1.0f);

        PushConstants pushConstants = {
            model,
            view,
            proj,
            modulatedColor,
            value,
            dimValue,
            wavePhase_,
            cycleProgress,
            static_cast<float>(cache_[i].darkMatter) * heavenGlow,
            static_cast<float>(cache_[i].darkEnergy) * heavenGlow
        };
        vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                          0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);

        std::cerr << "Mode5[D=" << i + 1 << "]: radius=" << radius << ", value=" << value
                  << ", pos=(" << pos.x << ", " << pos.y << ", " << pos.z << "), color=("
                  << modulatedColor.x << ", " << modulatedColor.y << ", " << modulatedColor.z << ")\n";
    }

    // Render interactions as glowing orbital paths
    for (size_t i = 0; i < kMaxRenderedDimensions; ++i) {
        ue_.setCurrentDimension(i + 1);
        auto pairs = ue_.getInteractions();
        if (pairs.empty()) {
            std::cerr << "Warning: No interactions for dimension " << i + 1 << "\n";
            continue;
        }

        for (const auto& pair : pairs) {
            if (pair.dimension != static_cast<int>(i + 1)) continue;

            float interactionStrength = static_cast<float>(
                computeInteraction(pair.dimension, pair.distance) *
                std::exp(-ue_.getAlpha() * pair.distance) *
                computePermeation(pair.dimension) *
                pair.darkMatterDensity);
            interactionStrength *= heavenGlow;
            if (interactionStrength < 0.01f) interactionStrength = 0.5f;

            float orbitRadius = 1.0f + static_cast<float>(pair.distance) * 0.3f * (1.0f + static_cast<float>(pair.darkMatterDensity) * 0.2f);
            float angle = wavePhase_ + pair.dimension * 2.0f;
            glm::vec3 orbitPos = glm::vec3(
                cosf(angle) * orbitRadius * zoomFactor,
                sinf(angle) * orbitRadius * zoomFactor,
                sinf(angle + pair.dimension) * orbitRadius * 0.5f * zoomFactor
            );
            glm::mat4 interactionModel = glm::translate(glm::mat4(1.0f), orbitPos);
            interactionModel = glm::scale(interactionModel, glm::vec3(0.3f * zoomFactor * (1.0f + heavenGlow)));

            glm::vec3 interactionColor = pinkBaseColor * (0.7f + 0.3f * static_cast<float>(pair.darkMatterDensity) * heavenGlow);
            interactionColor = glm::clamp(interactionColor, 0.0f, 1.0f);

            PushConstants pushConstants = {
                interactionModel,
                view,
                proj,
                interactionColor,
                interactionStrength,
                static_cast<float>(pair.dimension),
                wavePhase_,
                cycleProgress,
                static_cast<float>(pair.darkMatterDensity) * heavenGlow,
                static_cast<float>(computeDarkEnergy(pair.distance)) * heavenGlow
            };
            vkCmdPushConstants(commandBuffers_[imageIndex], pipelineLayout_,
                              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                              0, sizeof(PushConstants), &pushConstants);
            vkCmdDrawIndexed(commandBuffers_[imageIndex], static_cast<uint32_t>(sphereIndices_.size()), 1, 0, 0, 0);

            std::cerr << "Mode5 Interaction[D=" << pair.dimension << "]: strength=" << interactionStrength
                      << ", orbitRadius=" << orbitRadius << ", color=("
                      << interactionColor.x << ", " << interactionColor.y << ", " << interactionColor.z << ")\n";
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

        VkCommandBufferBeginInfo beginInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, 0, nullptr};
        if (vkBeginCommandBuffer(commandBuffers_[imageIndex], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("Failed to begin command buffer");
        }

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
        VkRenderPassBeginInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, renderPass_, swapchainFramebuffers_[imageIndex], {{0, 0}, {(uint32_t)width_, (uint32_t)height_}}, 1, &clearColor};
        vkCmdBeginRenderPass(commandBuffers_[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(commandBuffers_[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

        updateCache();
        switch (mode_) {
            case 1: renderMode1(imageIndex); break;
            case 2: renderMode2(imageIndex); break;
            case 3: renderMode3(imageIndex); break;
            case 4: renderMode4(imageIndex); break;
			case 5: renderMode5(imageIndex); break;
            default: renderMode1(imageIndex);
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
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
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