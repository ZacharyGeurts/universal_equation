// include/engine/Vulkan_utils.hpp
#pragma once
#ifndef VULKAN_UTILS_HPP
#define VULKAN_UTILS_HPP

#include <vulkan/vulkan.h>
#include <string>

namespace VulkanInitializer {
    std::string vkResultToString(VkResult result);
    std::string vkFormatToString(VkFormat format);
}

#endif // VULKAN_UTILS_HPP