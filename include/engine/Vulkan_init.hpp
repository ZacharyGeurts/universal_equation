// AMOURANTH RTX Engine, October 2025 - Vulkan core initialization.
// Initializes physical device, logical device, queues, command pool, and synchronization objects.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Usage: Create VulkanRenderer with instance, surface, and window dimensions; call beginFrame(), endFrame().
// Zachary Geurts 2025

#ifndef VULKAN_INIT_HPP
#define VULKAN_INIT_HPP

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <span>
#include <vector>
#include <string>

struct VulkanContext {
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    uint32_t graphicsFamily = 0;
    uint32_t presentFamily = 0;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkFramebuffer> swapchainFramebuffers;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexBufferMemory = VK_NULL_HANDLE;
    VkBuffer quadVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory quadVertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer quadIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory quadIndexBufferMemory = VK_NULL_HANDLE;
    VkBuffer voxelVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory voxelVertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer voxelIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory voxelIndexBufferMemory = VK_NULL_HANDLE;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkExtent2D swapchainExtent = {0, 0}; // Store swapchain dimensions
};

class VulkanRenderer {
public:
    VulkanRenderer(VkInstance instance, VkSurfaceKHR surface,
                  std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                  VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
                  int width, int height);
    ~VulkanRenderer();

    void beginFrame();
    void endFrame();
    VkShaderModule createShaderModule(const std::string& filename) const;
    void setShaderModules(VkShaderModule vertShaderModule, VkShaderModule fragShaderModule);
    void initializeVulkan(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices,
                         VkShaderModule vertShaderModule, VkShaderModule fragShaderModule,
                         int width, int height);
    void handleResize(int width, int height);
    void cleanupVulkan();

    uint32_t getCurrentImageIndex() const;
    VkBuffer getVertexBuffer() const;
    VkBuffer getIndexBuffer() const;
    VkCommandBuffer getCommandBuffer() const;
    VkPipelineLayout getPipelineLayout() const;
    VkDescriptorSet getDescriptorSet() const;
    const VulkanContext& getContext() const;

private:
    VulkanContext context_;
    VkInstance instance_;
    VkSurfaceKHR surface_;
    uint32_t currentFrame_ = 0;
    VkShaderModule vertShaderModule_ = VK_NULL_HANDLE;
    VkShaderModule fragShaderModule_ = VK_NULL_HANDLE;
};

#endif // VULKAN_INIT_HPP