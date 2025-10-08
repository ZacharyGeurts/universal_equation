// AMOURANTH RTX Engine, October 2025 - Vulkan buffer initialization.
// Manages vertex, index, quad, and voxel buffer creation.
// Dependencies: Vulkan 1.3+, GLM, C++20 standard library.
// Zachary Geurts 2025

#include "engine/Vulkan_init_buffers.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <cstring>
#include <format>

VulkanBufferManager::VulkanBufferManager(VulkanContext& context) : context_(context), logger_() {
    logger_.log(Logging::LogLevel::Debug, "Constructing VulkanBufferManager", std::source_location::current());
}

void VulkanBufferManager::initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    logger_.log(Logging::LogLevel::Debug, "Initializing vertex and index buffers with vertices.size={}, indices.size={}",
                std::source_location::current(), vertices.size(), indices.size());

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
        logger_.log(Logging::LogLevel::Error, "Failed to create vertex buffer", std::source_location::current());
        throw std::runtime_error("Failed to create vertex buffer");
    }
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(context_.device, context_.vertexBuffer, &memRequirements);
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(context_.physicalDevice, &memProperties);
    uint32_t memoryTypeIndex = 0;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
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
        logger_.log(Logging::LogLevel::Error, "Failed to allocate vertex buffer memory", std::source_location::current());
        throw std::runtime_error("Failed to allocate vertex buffer memory");
    }
    vkBindBufferMemory(context_.device, context_.vertexBuffer, context_.vertexBufferMemory, 0);
    void* data;
    vkMapMemory(context_.device, context_.vertexBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, vertices.data(), vertexBufferSize);
    vkUnmapMemory(context_.device, context_.vertexBufferMemory);
    logger_.log(Logging::LogLevel::Info, "Vertex buffer created with size={}", std::source_location::current(), vertexBufferSize);

    if (!indices.empty()) {
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
            logger_.log(Logging::LogLevel::Error, "Failed to create index buffer", std::source_location::current());
            throw std::runtime_error("Failed to create index buffer");
        }
        vkGetBufferMemoryRequirements(context_.device, context_.indexBuffer, &memRequirements);
        memAllocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = nullptr,
            .allocationSize = memRequirements.size,
            .memoryTypeIndex = memoryTypeIndex,
        };
        if (vkAllocateMemory(context_.device, &memAllocInfo, nullptr, &context_.indexBufferMemory) != VK_SUCCESS) {
            logger_.log(Logging::LogLevel::Error, "Failed to allocate index buffer memory", std::source_location::current());
            throw std::runtime_error("Failed to allocate index buffer memory");
        }
        vkBindBufferMemory(context_.device, context_.indexBuffer, context_.indexBufferMemory, 0);
        vkMapMemory(context_.device, context_.indexBufferMemory, 0, indexBufferSize, 0, &data);
        memcpy(data, indices.data(), indexBufferSize);
        vkUnmapMemory(context_.device, context_.indexBufferMemory);
        logger_.log(Logging::LogLevel::Info, "Index buffer created with size={}", std::source_location::current(), indexBufferSize);
    } else {
        logger_.log(Logging::LogLevel::Warning, "No indices provided, skipping index buffer creation", std::source_location::current());
    }
}

void VulkanBufferManager::initializeQuadBuffers(std::span<const glm::vec3> quadVertices, std::span<const uint32_t> quadIndices) {
    logger_.log(Logging::LogLevel::Debug, "Initializing quad buffers with vertices.size={}, indices.size={}",
                std::source_location::current(), quadVertices.size(), quadIndices.size());

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
        logger_.log(Logging::LogLevel::Error, "Failed to create quad vertex buffer", std::source_location::current());
        throw std::runtime_error("Failed to create quad vertex buffer");
    }
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(context_.device, context_.quadVertexBuffer, &memRequirements);
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(context_.physicalDevice, &memProperties);
    uint32_t memoryTypeIndex = 0;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
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
        logger_.log(Logging::LogLevel::Error, "Failed to allocate quad vertex buffer memory", std::source_location::current());
        throw std::runtime_error("Failed to allocate quad vertex buffer memory");
    }
    vkBindBufferMemory(context_.device, context_.quadVertexBuffer, context_.quadVertexBufferMemory, 0);
    void* data;
    vkMapMemory(context_.device, context_.quadVertexBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, quadVertices.data(), vertexBufferSize);
    vkUnmapMemory(context_.device, context_.quadVertexBufferMemory);
    logger_.log(Logging::LogLevel::Info, "Quad vertex buffer created with size={}", std::source_location::current(), vertexBufferSize);

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
        logger_.log(Logging::LogLevel::Error, "Failed to create quad index buffer", std::source_location::current());
        throw std::runtime_error("Failed to create quad index buffer");
    }
    vkGetBufferMemoryRequirements(context_.device, context_.quadIndexBuffer, &memRequirements);
    memAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex,
    };
    if (vkAllocateMemory(context_.device, &memAllocInfo, nullptr, &context_.quadIndexBufferMemory) != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to allocate quad index buffer memory", std::source_location::current());
        throw std::runtime_error("Failed to allocate quad index buffer memory");
    }
    vkBindBufferMemory(context_.device, context_.quadIndexBuffer, context_.quadIndexBufferMemory, 0);
    vkMapMemory(context_.device, context_.quadIndexBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, quadIndices.data(), indexBufferSize);
    vkUnmapMemory(context_.device, context_.quadIndexBufferMemory);
    logger_.log(Logging::LogLevel::Info, "Quad index buffer created with size={}", std::source_location::current(), indexBufferSize);
}

void VulkanBufferManager::initializeVoxelBuffers(std::span<const glm::vec3> voxelVertices, std::span<const uint32_t> voxelIndices) {
    logger_.log(Logging::LogLevel::Debug, "Initializing voxel buffers with vertices.size={}, indices.size={}",
                std::source_location::current(), voxelVertices.size(), voxelIndices.size());

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
        logger_.log(Logging::LogLevel::Error, "Failed to create voxel vertex buffer", std::source_location::current());
        throw std::runtime_error("Failed to create voxel vertex buffer");
    }
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(context_.device, context_.voxelVertexBuffer, &memRequirements);
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(context_.physicalDevice, &memProperties);
    uint32_t memoryTypeIndex = 0;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))) {
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
        logger_.log(Logging::LogLevel::Error, "Failed to allocate voxel vertex buffer memory", std::source_location::current());
        throw std::runtime_error("Failed to allocate voxel vertex buffer memory");
    }
    vkBindBufferMemory(context_.device, context_.voxelVertexBuffer, context_.voxelVertexBufferMemory, 0);
    void* data;
    vkMapMemory(context_.device, context_.voxelVertexBufferMemory, 0, vertexBufferSize, 0, &data);
    memcpy(data, voxelVertices.data(), vertexBufferSize);
    vkUnmapMemory(context_.device, context_.voxelVertexBufferMemory);
    logger_.log(Logging::LogLevel::Info, "Voxel vertex buffer created with size={}", std::source_location::current(), vertexBufferSize);

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
        logger_.log(Logging::LogLevel::Error, "Failed to create voxel index buffer", std::source_location::current());
        throw std::runtime_error("Failed to create voxel index buffer");
    }
    vkGetBufferMemoryRequirements(context_.device, context_.voxelIndexBuffer, &memRequirements);
    memAllocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = memoryTypeIndex,
    };
    if (vkAllocateMemory(context_.device, &memAllocInfo, nullptr, &context_.voxelIndexBufferMemory) != VK_SUCCESS) {
        logger_.log(Logging::LogLevel::Error, "Failed to allocate voxel index buffer memory", std::source_location::current());
        throw std::runtime_error("Failed to allocate voxel index buffer memory");
    }
    vkBindBufferMemory(context_.device, context_.voxelIndexBuffer, context_.voxelIndexBufferMemory, 0);
    vkMapMemory(context_.device, context_.voxelIndexBufferMemory, 0, indexBufferSize, 0, &data);
    memcpy(data, voxelIndices.data(), indexBufferSize);
    vkUnmapMemory(context_.device, context_.voxelIndexBufferMemory);
    logger_.log(Logging::LogLevel::Info, "Voxel index buffer created with size={}", std::source_location::current(), indexBufferSize);
}

void VulkanBufferManager::cleanupBuffers() {
    logger_.log(Logging::LogLevel::Info, "Cleaning up buffers", std::source_location::current());

    if (context_.vertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.vertexBuffer, nullptr);
        context_.vertexBuffer = VK_NULL_HANDLE;
    }
    if (context_.vertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.vertexBufferMemory, nullptr);
        context_.vertexBufferMemory = VK_NULL_HANDLE;
    }
    if (context_.indexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.indexBuffer, nullptr);
        context_.indexBuffer = VK_NULL_HANDLE;
    }
    if (context_.indexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.indexBufferMemory, nullptr);
        context_.indexBufferMemory = VK_NULL_HANDLE;
    }
    if (context_.quadVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.quadVertexBuffer, nullptr);
        context_.quadVertexBuffer = VK_NULL_HANDLE;
    }
    if (context_.quadVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.quadVertexBufferMemory, nullptr);
        context_.quadVertexBufferMemory = VK_NULL_HANDLE;
    }
    if (context_.quadIndexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.quadIndexBuffer, nullptr);
        context_.quadIndexBuffer = VK_NULL_HANDLE;
    }
    if (context_.quadIndexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.quadIndexBufferMemory, nullptr);
        context_.quadIndexBufferMemory = VK_NULL_HANDLE;
    }
    if (context_.voxelVertexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.voxelVertexBuffer, nullptr);
        context_.voxelVertexBuffer = VK_NULL_HANDLE;
    }
    if (context_.voxelVertexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.voxelVertexBufferMemory, nullptr);
        context_.voxelVertexBufferMemory = VK_NULL_HANDLE;
    }
    if (context_.voxelIndexBuffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(context_.device, context_.voxelIndexBuffer, nullptr);
        context_.voxelIndexBuffer = VK_NULL_HANDLE;
    }
    if (context_.voxelIndexBufferMemory != VK_NULL_HANDLE) {
        vkFreeMemory(context_.device, context_.voxelIndexBufferMemory, nullptr);
        context_.voxelIndexBufferMemory = VK_NULL_HANDLE;
    }
    logger_.log(Logging::LogLevel::Info, "Buffers cleaned up", std::source_location::current());
}