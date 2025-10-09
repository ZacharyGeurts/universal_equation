// Vulkan_func.cpp
// Utility functions for Vulkan in AMOURANTH RTX Engine
// AMOURANTH RTX Engine, October 2025
// Zachary Geurts 2025

#include "engine/Vulkan_init.hpp" // For VulkanInitializer declarations
#include "engine/Vulkan/Vulkan_func_pipe.hpp"
#include "engine/Vulkan/Vulkan_func_swapchain.hpp"
#include "engine/logging.hpp"
#include <vulkan/vulkan.h>
#include <stdexcept>
#include <cstring> // For strcmp
#include <source_location>

namespace VulkanUtils {
// Utility function to check if a Vulkan extension is supported by the physical device
bool isExtensionSupported(VkPhysicalDevice physicalDevice, const char* extensionName) {
    LOG_DEBUG_CAT("Vulkan", "Checking support for extension: {}", std::source_location::current(), extensionName);

    uint32_t extCount = 0;
    VkResult result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to enumerate device extensions: VkResult {}", std::source_location::current(), static_cast<int>(result));
        return false;
    }

    std::vector<VkExtensionProperties> availableExts(extCount);
    result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, availableExts.data());
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "Failed to retrieve device extensions: VkResult {}", std::source_location::current(), static_cast<int>(result));
        return false;
    }

    for (const auto& ext : availableExts) {
        if (strcmp(ext.extensionName, extensionName) == 0) { // Removed std:: prefix
            LOG_DEBUG_CAT("Vulkan", "Extension {} is supported", std::source_location::current(), extensionName);
            return true;
        }
    }

    LOG_WARNING_CAT("Vulkan", "Extension {} is not supported", std::source_location::current(), extensionName);
    return false;
}

// Utility function to get the memory type index for a given set of properties
bool findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties, uint32_t& memoryTypeIndex) {
    LOG_DEBUG_CAT("Vulkan", "Finding memory type with filter={:x} and properties={:x}", std::source_location::current(), typeFilter, properties);

    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            memoryTypeIndex = i;
            LOG_DEBUG_CAT("Vulkan", "Found memory type index: {}", std::source_location::current(), i);
            return true;
        }
    }

    LOG_ERROR_CAT("Vulkan", "Failed to find suitable memory type", std::source_location::current());
    return false;
}
} // namespace VulkanUtils