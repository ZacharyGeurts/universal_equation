#include "VulkanBufferManager.hpp"
#include "engine/logging.hpp"

VulkanBufferManager::VulkanBufferManager(VulkanContext& context)
    : context_(context), vertexBuffer_(VK_NULL_HANDLE), vertexBufferMemory_(VK_NULL_HANDLE), vertexBufferAddress_(0),
      indexBuffer_(VK_NULL_HANDLE), indexBufferMemory_(VK_NULL_HANDLE), indexBufferAddress_(0),
      scratchBuffer_(VK_NULL_HANDLE), scratchBufferMemory_(VK_NULL_HANDLE), scratchBufferAddress_(0), indexCount_(0) {
    LOG_INFO("Initialized VulkanBufferManager");
}

VulkanBufferManager::~VulkanBufferManager() {
    cleanupBuffers();
}

void VulkanBufferManager::initializeBuffers(std::span<const glm::vec3> vertices, std::span<const uint32_t> indices) {
    VkDeviceSize vertexBufferSize = sizeof(glm::vec3) * vertices.size();
    VkDeviceSize indexBufferSize = sizeof(uint32_t) * indices.size();
    indexCount_ = static_cast<uint32_t>(indices.size());

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(vertexBufferSize + indexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingBufferMemory);

    void* data;
    vkMapMemory(context_.device, stagingBufferMemory, 0, vertexBufferSize + indexBufferSize, 0, &data);
    memcpy(data, vertices.data(), vertexBufferSize);
    memcpy(static_cast<char*>(data) + vertexBufferSize, indices.data(), indexBufferSize);
    vkUnmapMemory(context_.device, stagingBufferMemory);

    createBuffer(vertexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 vertexBuffer_, vertexBufferMemory_);
    vertexBufferAddress_ = getBufferDeviceAddress(vertexBuffer_);

    createBuffer(indexBufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 indexBuffer_, indexBufferMemory_);
    indexBufferAddress_ = getBufferDeviceAddress(indexBuffer_);

    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = context_.commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer commandBuffer;
    if (vkAllocateCommandBuffers(context_.device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate command buffer", std::source_location::current());
        return;
    }
    LOG_INFO_CAT("Vulkan", commandBuffer, "commandBuffer", std::source_location::current());

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to begin command buffer", std::source_location::current());
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &commandBuffer);
        return;
    }

    VkBufferCopy vertexCopyRegion{
        .srcOffset = 0,
        .dstOffset = 0,
        .size = vertexBufferSize
    };
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, vertexBuffer_, 1, &vertexCopyRegion);

    VkBufferCopy indexCopyRegion{
        .srcOffset = vertexBufferSize,
        .dstOffset = 0,
        .size = indexBufferSize
    };
    vkCmdCopyBuffer(commandBuffer, stagingBuffer, indexBuffer_, 1, &indexCopyRegion);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to end command buffer", std::source_location::current());
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &commandBuffer);
        return;
    }

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &commandBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
    };
    if (vkQueueSubmit(context_.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to submit command buffer", std::source_location::current());
        vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &commandBuffer);
        return;
    }

    vkQueueWaitIdle(context_.graphicsQueue);
    vkFreeCommandBuffers(context_.device, context_.commandPool, 1, &commandBuffer);
    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingBufferMemory, nullptr);

    LOG_INFO_CAT("Vulkan", vertexBuffer_, "vertexBuffer", std::source_location::current());
    LOG_INFO_CAT("Vulkan", indexBuffer_, "indexBuffer", std::source_location::current());
}

void VulkanBufferManager::createUniformBuffers(uint32_t count) {
    uniformBuffers_.resize(count);
    uniformBufferMemories_.resize(count);
    VkDeviceSize bufferSize = sizeof(UE::UniformBufferObject);
    for (uint32_t i = 0; i < count; ++i) {
        createBuffer(bufferSize,
                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                     uniformBuffers_[i], uniformBufferMemories_[i]);
        LOG_INFO_CAT("Vulkan", uniformBuffers_[i], "uniformBuffer", std::source_location::current());
    }
}

void VulkanBufferManager::createScratchBuffer(VkDeviceSize size) {
    createBuffer(size,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 scratchBuffer_, scratchBufferMemory_);
    scratchBufferAddress_ = getBufferDeviceAddress(scratchBuffer_);
    LOG_INFO_CAT("Vulkan", scratchBuffer_, "scratchBuffer", std::source_location::current());
}

void VulkanBufferManager::cleanupBuffers() {
    for (auto buffer : uniformBuffers_) {
        if (buffer != VK_NULL_HANDLE) {
            LOG_INFO_CAT("Vulkan", buffer, "uniformBuffer", std::source_location::current());
            vkDestroyBuffer(context_.device, buffer, nullptr);
        }
    }
    for (auto memory : uniformBufferMemories_) {
        if (memory != VK_NULL_HANDLE) {
            LOG_INFO_CAT("Vulkan", memory, "uniformBufferMemory", std::source_location::current());
            vkFreeMemory(context_.device, memory, nullptr);
        }
    }
    if (vertexBuffer_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", vertexBuffer_, "vertexBuffer", std::source_location::current());
        vkDestroyBuffer(context_.device, vertexBuffer_, nullptr);
    }
    if (vertexBufferMemory_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", vertexBufferMemory_, "vertexBufferMemory", std::source_location::current());
        vkFreeMemory(context_.device, vertexBufferMemory_, nullptr);
    }
    if (indexBuffer_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", indexBuffer_, "indexBuffer", std::source_location::current());
        vkDestroyBuffer(context_.device, indexBuffer_, nullptr);
    }
    if (indexBufferMemory_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", indexBufferMemory_, "indexBufferMemory", std::source_location::current());
        vkFreeMemory(context_.device, indexBufferMemory_, nullptr);
    }
    if (scratchBuffer_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", scratchBuffer_, "scratchBuffer", std::source_location::current());
        vkDestroyBuffer(context_.device, scratchBuffer_, nullptr);
    }
    if (scratchBufferMemory_ != VK_NULL_HANDLE) {
        LOG_INFO_CAT("Vulkan", scratchBufferMemory_, "scratchBufferMemory", std::source_location::current());
        vkFreeMemory(context_.device, scratchBufferMemory_, nullptr);
    }
}

void VulkanBufferManager::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                                      VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };
    if (vkCreateBuffer(context_.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to create buffer size: {}", std::source_location::current(), size);
        return;
    }
    LOG_INFO_CAT("Vulkan", buffer, "buffer", std::source_location::current());

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(context_.device, buffer, &memRequirements);

    VkMemoryAllocateFlagsInfo allocFlagsInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = nullptr,
        .flags = static_cast<VkMemoryAllocateFlags>(usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT ? VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT_KHR : 0),
        .deviceMask = 0
    };
    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &allocFlagsInfo : nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)
    };
    if (allocInfo.memoryTypeIndex == UINT32_MAX || vkAllocateMemory(context_.device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to allocate buffer memory size: {}", std::source_location::current(), memRequirements.size);
        vkDestroyBuffer(context_.device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        return;
    }
    LOG_INFO_CAT("Vulkan", bufferMemory, "bufferMemory", std::source_location::current());
    vkBindBufferMemory(context_.device, buffer, bufferMemory, 0);
}

uint32_t VulkanBufferManager::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(context_.physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            LOG_DEBUG_CAT("Vulkan", "Memory type index: {}", std::source_location::current(), i);
            return i;
        }
    }
    LOG_ERROR_CAT("Vulkan", "Failed to find memory type filter: {}, properties: {}", std::source_location::current(), typeFilter, properties);
    return UINT32_MAX;
}

VkDeviceAddress VulkanBufferManager::getBufferDeviceAddress(VkBuffer buffer) const {
    auto vkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(context_.device, "vkGetBufferDeviceAddressKHR");
    if (!vkGetBufferDeviceAddressKHR) {
        LOG_ERROR_CAT("Vulkan", "Failed to get vkGetBufferDeviceAddressKHR", std::source_location::current());
        return 0;
    }
    VkBufferDeviceAddressInfo addressInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = buffer
    };
    VkDeviceAddress address = vkGetBufferDeviceAddressKHR(context_.device, &addressInfo);
    LOG_DEBUG_CAT("Vulkan", buffer, "buffer", std::source_location::current());
    return address;
}