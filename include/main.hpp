#ifndef MAIN_HPP
#define MAIN_HPP

// AMOURANTH RTX Engine, October 2025 - Main application class for Vulkan-based rendering.
// Manages initialization, rendering, and input handling for the Universal Equation simulation.
// Supports multiple rendering modes (1-9), with mode1 implemented for instanced sphere rendering.
// Dependencies: Vulkan 1.3+, SDL3, GLM, C++20 standard library.
// Usage: Create Application instance, call initialize(), then run() in a loop.
// Zachary Geurts 2025

#include "engine/core.hpp"
#include "engine/Vulkan_init.hpp"
#include "engine/SDL3_init.hpp"
#include "engine/handleinput.hpp"
#include <vector>
#include <memory>

class Application {
public:
    Application(const char* title, int width, int height)
        : title_(title), width_(width), height_(height), mode_(1) {}

    void initialize() {
        sdl_ = std::make_unique<SDL3Initializer>(title_, width_, height_);
        vertices_ = {
            glm::vec3(-0.5f, -0.5f, 0.0f),
            glm::vec3(0.5f, -0.5f, 0.0f),
            glm::vec3(0.0f, 0.5f, 0.0f)
        };
        indices_ = {0, 1, 2};

        renderer_ = std::make_unique<VulkanRenderer>(
            sdl_->getInstance(), sdl_->getSurface(),
            vertices_, indices_,
            width_, height_
        );

        VkShaderModule vertShaderModule = renderer_->createShaderModule("shaders/vertex.spv");
        VkShaderModule fragShaderModule = renderer_->createShaderModule("shaders/fragment.spv");

        // Update renderer with shader modules
        renderer_->setShaderModules(vertShaderModule, fragShaderModule);

        vkDestroyShaderModule(renderer_->getContext().device, vertShaderModule, nullptr);
        vkDestroyShaderModule(renderer_->getContext().device, fragShaderModule, nullptr);
    }

    void run() {
        while (!sdl_->shouldQuit()) {
            sdl_->pollEvents();
            inputHandler_.handleInput(*this);
            render();
        }
    }

    void render() {
        renderer_->beginFrame();
        AMOURANTH::render(
            renderer_->getCurrentImageIndex(),
            renderer_->getVertexBuffer(),
            renderer_->getCommandBuffer(),
            renderer_->getIndexBuffer(),
            renderer_->getPipelineLayout(),
            renderer_->getDescriptorSet()
        );
        renderer_->endFrame();
    }

    void setRenderMode(int mode) { mode_ = mode; }
    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

private:
    std::string title_;
    int width_, height_;
    int mode_;
    std::unique_ptr<SDL3Initializer> sdl_;
    std::unique_ptr<VulkanRenderer> renderer_;
    InputHandler inputHandler_;
    std::vector<glm::vec3> vertices_;
    std::vector<uint32_t> indices_;
};

#endif // MAIN_HPP