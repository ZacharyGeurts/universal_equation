// AMOURANTH RTX Engine, October 2025 - Vulkan buffer initialization.
// Manages vertex, index, quad, and voxel buffer creation.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#include "engine/Vulkan_init_buffers.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan_init.hpp"
#include <stdexcept>
#include <cstring>
#include <format>

VulkanBufferManager::VulkanBufferManager(VulkanContext& context) : context_(context) {}

void VulkanBufferManager::initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Debug, "Initializing vertex and index buffers", std::source_location::current());

    // Create vertex buffer
    VkDeviceSize vertexBufferSize = vertices.size() * sizeof(glm::vec3);
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = vertexBufferSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };
    if (vkCreateBuffer(context_.device, &bufferInfo, nullptr, &context_.vertexBuffer) != VK_SUCCESS) {
        std::string error = "Failed to create vertex buffer";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(context_.device, context_.vertexBuffer, &memRequirements);
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(context_.physicalDevice, &memProperties);
    uint32_t memoryTypeIndex = 0;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            memoryTypeIndex = i;
            break;
        }
    }
    VkMemoryAllocateInfo memAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex,
    };
    if (vkAllocateMemory(context_.device, &memAllocInfo, nullptr, &context_.vertexBufferMemory) != VK_SUCCESS) {
        std::string error = "Failed to allocate vertex buffer memory";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    vkBindBufferMemory(context_.device, context_.vertexBuffer, context_.vertexBufferMemory, 0);
    void* data;
    vkMapMemory(context_.device, context_.vertexBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, vertices.data(), vertexBufferSize);
    vkUnmapMemory(context_.device, context_.vertexBufferMemory);
    logger.log(Logging::LogLevel::Info, "Vertex buffer created", std::source_location::current());

    // Create index buffer
    VkDeviceSize indexBufferSize = indices.size() * sizeof(uint32_t);
    bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = indexBufferSize,
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };
    if (vkCreateBuffer(context_.device, &bufferInfo, nullptr, &context_.indexBuffer) != VK_SUCCESS) {
        std::string error = "Failed to create index buffer";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    vkGetBufferMemoryRequirements(context_.device, context_.indexBuffer, &memRequirements);
    memAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex,
    };
    if (vkAllocateMemory(context_.device, &memAllocInfo, nullptr, &context_.indexBufferMemory) != VK_SUCCESS) {
        std::string error = "Failed to allocate index buffer memory";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    vkBindBufferMemory(context_.device, context_.indexBuffer, context_.indexBufferMemory, 0);
    vkMapMemory(context_.device, context_.indexBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, indices.data(), indexBufferSize);
    vkUnmapMemory(context_.device, context_.indexBufferMemory);
    logger.log(Logging::LogLevel::Info, "Index buffer created", std::source_location::current());
}

void VulkanBufferManager::initializeQuadBuffers(std::span<const glm::vec3> quadVertices, std::span<const uint32_t> quadIndices) {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Debug, "Initializing quad buffers", std::source_location::current());

    VkDeviceSize vertexBufferSize = quadVertices.size() * sizeof(glm::vec3);
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = vertexBufferSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };
    if (vkCreateBuffer(context_.device, &bufferInfo, nullptr, &context_.quadVertexBuffer) != VK_SUCCESS) {
        std::string error = "Failed to create quad vertex buffer";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(context_.device, context_.quadVertexBuffer, &memRequirements);
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(context_.physicalDevice, &memProperties);
    uint32_t memoryTypeIndex = 0;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            memoryTypeIndex = i;
            break;
        }
    }
    VkMemoryAllocateInfo memAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex,
    };
    if (vkAllocateMemory(context_.device, &memAllocInfo, nullptr, &context_.quadVertexBufferMemory) != VK_SUCCESS) {
        std::string error = "Failed to allocate quad vertex buffer memory";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    vkBindBufferMemory(context_.device, context_.quadVertexBuffer, context_.quadVertexBufferMemory, 0);
    void* data;
    vkMapMemory(context_.device, context_.quadVertexBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, quadVertices.data(), vertexBufferSize);
    vkUnmapMemory(context_.device, context_.quadVertexBufferMemory);
    logger.log(Logging::LogLevel::Info, "Quad vertex buffer created", std::source_location::current());

    VkDeviceSize indexBufferSize = quadIndices.size() * sizeof(uint32_t);
    bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = indexBufferSize,
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };
    if (vkCreateBuffer(context_.device, &bufferInfo, nullptr, &context_.quadIndexBuffer) != VK_SUCCESS) {
        std::string error = "Failed to create quad index buffer";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    vkGetBufferMemoryRequirements(context_.device, context_.quadIndexBuffer, &memRequirements);
    memAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex,
    };
    if (vkAllocateMemory(context_.device, &memAllocInfo, nullptr, &context_.quadIndexBufferMemory) != VK_SUCCESS) {
        std::string error = "Failed to allocate quad index buffer memory";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    vkBindBufferMemory(context_.device, context_.quadIndexBuffer, context_.quadIndexBufferMemory, 0);
    vkMapMemory(context_.device, context_.quadIndexBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, quadIndices.data(), indexBufferSize);
    vkUnmapMemory(context_.device, context_.quadIndexBufferMemory);
    logger.log(Logging::LogLevel::Info, "Quad index buffer created", std::source_location::current());
}

void VulkanBufferManager::initializeVoxelBuffers(std::span<const glm::vec3> voxelVertices, std::span<const uint32_t> voxelIndices) {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Debug, "Initializing voxel buffers", std::source_location::current());

    VkDeviceSize vertexBufferSize = voxelVertices.size() * sizeof(glm::vec3);
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = vertexBufferSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };
    if (vkCreateBuffer(context_.device, &bufferInfo, nullptr, &context_.voxelVertexBuffer) != VK_SUCCESS) {
        std::string error = "Failed to create voxel vertex buffer";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(context_.device, context_.voxelVertexBuffer, &memRequirements);
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(context_.physicalDevice, &memProperties);
    uint32_t memoryTypeIndex = 0;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
            memoryTypeIndex = i;
            break;
        }
    }
    VkMemoryAllocateInfo memAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex,
    };
    if (vkAllocateMemory(context_.device, &memAllocInfo, nullptr, &context_.voxelVertexBufferMemory) != VK_SUCCESS) {
        std::string error = "Failed to allocate voxel vertex buffer memory";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    vkBindBufferMemory(context_.device, context_.voxelVertexBuffer, context_.voxelVertexBufferMemory, 0);
    void* data;
    vkMapMemory(context_.device, context_.voxelVertexBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, voxelVertices.data(), vertexBufferSize);
    vkUnmapMemory(context_.device, context_.voxelVertexBufferMemory);
    logger.log(Logging::LogLevel::Info, "Voxel vertex buffer created", std::source_location::current());

    VkDeviceSize indexBufferSize = voxelIndices.size() * sizeof(uint32_t);
    bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = indexBufferSize,
        .usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };
    if (vkCreateBuffer(context_.device, &bufferInfo, nullptr, &context_.voxelIndexBuffer) != VK_SUCCESS) {
        std::string error = "Failed to create voxel index buffer";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    vkGetBufferMemoryRequirements(context_.device, context_.voxelIndexBuffer, &memRequirements);
    memAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex,
    };
    if (vkAllocateMemory(context_.device, &memAllocInfo, nullptr, &context_.voxelIndexBufferMemory) != VK_SUCCESS) {
        std::string error = "Failed to allocate voxel index buffer memory";
        logger.log(Logging::LogLevel::Error, "{}", std::source_location::current(), error);
        throw std::runtime_error(error);
    }
    vkBindBufferMemory(context_.device, context_.voxelIndexBuffer, context_.voxelIndexBufferMemory, 0);
    vkMapMemory(context_.device, context_.voxelIndexBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, voxelIndices.data(), indexBufferSize);
    vkUnmapMemory(context_.device, context_.voxelIndexBufferMemory);
    logger.log(Logging::LogLevel::Info, "Voxel index buffer created", std::source_location::current());
}

void VulkanBufferManager::cleanupBuffers() {
    Logging::Logger logger;
    logger.log(Logging::LogLevel::Info, "Cleaning up buffers", std::source_location::current());

    vkDestroyBuffer(context_.device, context_.vertexBuffer, nullptr);
    vkFreeMemory(context_.device, context_.vertexBufferMemory, nullptr);
    vkDestroyBuffer(context_.device, context_.indexBuffer, nullptr);
    vkFreeMemory(context_.device, context_.indexBufferMemory, nullptr);
    vkDestroyBuffer(context_.device, context_.quadVertexBuffer, nullptr);
    vkFreeMemory(context_.device, context_.quadVertexBufferMemory, nullptr);
    vkDestroyBuffer(context_.device, context_.quadIndexBuffer, nullptr);
    vkFreeMemory(context_.device, context_.quadIndexBufferMemory, nullptr);
    vkDestroyBuffer(context_.device, context_.voxelVertexBuffer, nullptr);
    vkFreeMemory(context_.device, context_.voxelVertexBufferMemory, nullptr);
    vkDestroyBuffer(context_.device, context_.voxelIndexBuffer, nullptr);
    vkFreeMemory(context_.device, context_.voxelIndexBufferMemory, nullptr);
    logger.log(Logging::LogLevel::Info, "Buffers cleaned up", std::source_location::current());
}