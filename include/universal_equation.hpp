// include/universal_equation.hpp
#pragma once
#ifndef UNIVERSAL_EQUATION_HPP
#define UNIVERSAL_EQUATION_HPP

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp> // Added for translate and scale

class DimensionalNavigator; // Forward declaration

class AMOURANTH {
public:
    AMOURANTH(DimensionalNavigator* navigator, VkDevice device, VkDeviceMemory vertexBufferMemory, VkPipeline pipeline)
        : navigator_(navigator), device_(device), vertexBufferMemory_(vertexBufferMemory), pipeline_(pipeline),
          mode_(1), zoom_(1.0f), influence_(1.0f), nurbMatter_(1.0f), nurbEnergy_(1.0f),
          isPaused_(false), isUserCamActive_(false), userCamPosition_(0.0f), transform_(glm::mat4(1.0f)) {}

    void setMode(int mode) { mode_ = mode; }
    void updateZoom(bool zoomIn) { zoom_ += zoomIn ? 0.1f : -0.1f; }
    void adjustInfluence(float delta) { influence_ += delta; }
    void adjustNurbMatter(float delta) { nurbMatter_ += delta; }
    void adjustNurbEnergy(float delta) { nurbEnergy_ += delta; }
    void togglePause() { isPaused_ = !isPaused_; }
    void toggleUserCam() { isUserCamActive_ = !isUserCamActive_; }
    bool isUserCamActive() const { return isUserCamActive_; }
    void moveUserCam(float dx, float dy, float dz) {
        userCamPosition_ += glm::vec3(dx, dy, dz);
        updateTransform();
    }
    void setWidth(int width) { width_ = width; updateTransform(); }
    void setHeight(int height) { height_ = height; updateTransform(); }
    glm::mat4 getTransform() const { return transform_; }

private:
    void updateTransform() {
        transform_ = glm::mat4(1.0f);
        transform_ = glm::translate(transform_, userCamPosition_);
        transform_ = glm::scale(transform_, glm::vec3(zoom_));
    }

    DimensionalNavigator* navigator_;
    VkDevice device_;
    VkDeviceMemory vertexBufferMemory_;
    VkPipeline pipeline_;
    int mode_;
    float zoom_;
    float influence_;
    float nurbMatter_;
    float nurbEnergy_;
    bool isPaused_;
    bool isUserCamActive_;
    glm::vec3 userCamPosition_;
    glm::mat4 transform_;
    int width_ = 800;
    int height_ = 600;
};

#endif // UNIVERSAL_EQUATION_HPP