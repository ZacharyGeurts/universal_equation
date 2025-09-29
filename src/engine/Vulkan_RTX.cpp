#include "Vulkan_RTX.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <cstring>
#include <array>
#include <future>
// AMOURANTH RTX Engine - September 2025
// Author: Zachary Geurts, 2025

std::mutex VulkanRTX::functionPtrMutex_;
std::mutex VulkanRTX::shaderModuleMutex_;
PFN_vkGetBufferDeviceAddress VulkanRTX::vkGetBufferDeviceAddress = nullptr;
PFN_vkCmdTraceRaysKHR VulkanRTX::vkCmdTraceRaysKHR = nullptr;
PFN_vkCreateAccelerationStructureKHR VulkanRTX::vkCreateAccelerationStructureKHR = nullptr;
PFN_vkDestroyAccelerationStructureKHR VulkanRTX::vkDestroyASFunc = nullptr;
PFN_vkGetAccelerationStructureBuildSizesKHR VulkanRTX::vkGetAccelerationStructureBuildSizesKHR = nullptr;
PFN_vkCmdBuildAccelerationStructuresKHR VulkanRTX::vkCmdBuildAccelerationStructuresKHR = nullptr;
PFN_vkGetAccelerationStructureDeviceAddressKHR VulkanRTX::vkGetAccelerationStructureDeviceAddressKHR = nullptr;
PFN_vkCreateRayTracingPipelinesKHR VulkanRTX::vkCreateRayTracingPipelinesKHR = nullptr;
PFN_vkGetRayTracingShaderGroupHandlesKHR VulkanRTX::vkGetRayTracingShaderGroupHandlesKHR = nullptr;
PFN_vkCmdCopyAccelerationStructureKHR VulkanRTX::vkCmdCopyAccelerationStructureKHR = nullptr;

VulkanRTX::VulkanRTX(VkDevice device, const std::vector<std::string>& shaderPaths)
    : device_(device),
      shaderPaths_(shaderPaths),
      dimensionCache_(),
      dsLayout_(device, VK_NULL_HANDLE),
      dsPool_(device, VK_NULL_HANDLE),
      ds_(device, VK_NULL_HANDLE, VK_NULL_HANDLE),
      rtPipelineLayout_(device, VK_NULL_HANDLE),
      rtPipeline_(device, VK_NULL_HANDLE),
      blas_(device, VK_NULL_HANDLE),
      blasBuffer_(device, VK_NULL_HANDLE),
      blasMemory_(device, VK_NULL_HANDLE),
      tlas_(device, VK_NULL_HANDLE),
      tlasBuffer_(device, VK_NULL_HANDLE),
      tlasMemory_(device, VK_NULL_HANDLE),
      sbt_(device) {
    std::lock_guard<std::mutex> lock(functionPtrMutex_);
    vkGetBufferDeviceAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(vkGetDeviceProcAddr(device_, "vkGetBufferDeviceAddress"));
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device_, "vkCmdTraceRaysKHR"));
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device_, "vkCreateAccelerationStructureKHR"));
    vkDestroyASFunc = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device_, "vkDestroyAccelerationStructureKHR"));
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device_, "vkGetAccelerationStructureBuildSizesKHR"));
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device_, "vkCmdBuildAccelerationStructuresKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device_, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device_, "vkCreateRayTracingPipelinesKHR"));
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device_, "vkGetRayTracingShaderGroupHandlesKHR"));
    vkCmdCopyAccelerationStructureKHR = reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(vkGetDeviceProcAddr(device_, "vkCmdCopyAccelerationStructureKHR"));

    if (!vkGetBufferDeviceAddress || !vkCmdTraceRaysKHR || !vkCreateAccelerationStructureKHR ||
        !vkDestroyASFunc || !vkGetAccelerationStructureBuildSizesKHR ||
        !vkCmdBuildAccelerationStructuresKHR || !vkGetAccelerationStructureDeviceAddressKHR ||
        !vkCreateRayTracingPipelinesKHR || !vkGetRayTracingShaderGroupHandlesKHR) {
        spdlog::error("Failed to load ray tracing extension functions: vkDestroyAccelerationStructureKHR = {}",
                      reinterpret_cast<void*>(vkDestroyASFunc));
        throw VulkanRTXException("Failed to load ray tracing extension functions");
    }
    supportsCompaction_ = vkCmdCopyAccelerationStructureKHR != nullptr;
    spdlog::info("VulkanRTX initialized; compaction support: {}", supportsCompaction_);
}

void VulkanRTX::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding bindings[5] = {};
    bindings[static_cast<uint32_t>(DescriptorBindings::TLAS)] = {
        0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
        VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        nullptr
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::StorageImage)] = {
        1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR, nullptr
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::CameraUBO)] = {
        2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | 
        VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR, nullptr
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::MaterialSSBO)] = {
        3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | 
        VK_SHADER_STAGE_CALLABLE_BIT_KHR, nullptr
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO)] = {
        4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | 
        VK_SHADER_STAGE_CALLABLE_BIT_KHR, nullptr
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = { 
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0, 5, bindings 
    };
    VK_CHECK(vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &dsLayout_.get()), 
             "Failed to create descriptor set layout");
}

void VulkanRTX::createDescriptorPoolAndSet() {
    VkDescriptorPoolSize poolSizes[5] = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 }
    };
    VkDescriptorPoolCreateInfo poolInfo = { 
        VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO, nullptr, 0, 1, 5, poolSizes 
    };
    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &dsPool_.get()), 
             "Failed to create descriptor pool");

    VkDescriptorSetAllocateInfo allocInfo = { 
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, nullptr, dsPool_.get(), 1, &dsLayout_.get() 
    };
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet), 
             "Failed to allocate descriptor set");
    ds_ = VulkanDescriptorSet(device_, dsPool_.get(), descriptorSet);
}

void VulkanRTX::createRayTracingPipeline(uint32_t maxRayRecursionDepth) {
    std::vector<VkShaderModule> shaderModules(shaderPaths_.size(), VK_NULL_HANDLE);
    loadShadersAsync(shaderModules, shaderPaths_);

    shaderFeatures_ = ShaderFeatures::None;
    if (shaderModules[3] != VK_NULL_HANDLE) shaderFeatures_ = static_cast<ShaderFeatures>(
        static_cast<uint32_t>(shaderFeatures_) | static_cast<uint32_t>(ShaderFeatures::AnyHit));
    if (shaderModules[4] != VK_NULL_HANDLE) shaderFeatures_ = static_cast<ShaderFeatures>(
        static_cast<uint32_t>(shaderFeatures_) | static_cast<uint32_t>(ShaderFeatures::Intersection));
    if (shaderModules[5] != VK_NULL_HANDLE) shaderFeatures_ = static_cast<ShaderFeatures>(
        static_cast<uint32_t>(shaderFeatures_) | static_cast<uint32_t>(ShaderFeatures::Callable));

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, 
          VK_SHADER_STAGE_RAYGEN_BIT_KHR, shaderModules[0], "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, 
          VK_SHADER_STAGE_MISS_BIT_KHR, shaderModules[1], "main", nullptr },
        { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, 
          VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, shaderModules[2], "main", nullptr }
    };
    if (hasShaderFeature(ShaderFeatures::AnyHit)) {
        stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, 
                           VK_SHADER_STAGE_ANY_HIT_BIT_KHR, shaderModules[3], "main", nullptr });
    }
    if (hasShaderFeature(ShaderFeatures::Intersection)) {
        stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, 
                           VK_SHADER_STAGE_INTERSECTION_BIT_KHR, shaderModules[4], "main", nullptr });
    }
    if (hasShaderFeature(ShaderFeatures::Callable)) {
        stages.push_back({ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, 
                           VK_SHADER_STAGE_CALLABLE_BIT_KHR, shaderModules[5], "main", nullptr });
    }

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    buildShaderGroups(groups, stages);

    VkPushConstantRange pushConstant = {
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | 
        VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | 
        VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        0, sizeof(PushConstants)
    };
    VkPipelineLayoutCreateInfo layoutInfo = { 
        VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, nullptr, 0, 1, &dsLayout_.get(), 1, &pushConstant 
    };
    VK_CHECK(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &rtPipelineLayout_.get()), 
             "Failed to create pipeline layout");

    VkRayTracingPipelineCreateInfoKHR createInfo = {
        VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        nullptr, 0, static_cast<uint32_t>(stages.size()), stages.data(),
        static_cast<uint32_t>(groups.size()), groups.data(),
        maxRayRecursionDepth, nullptr, nullptr, nullptr, rtPipelineLayout_.get(), VK_NULL_HANDLE, 0
    };
    VK_CHECK(vkCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &createInfo, nullptr, &rtPipeline_.get()), 
             "Failed to create ray tracing pipeline");

    for (auto module : shaderModules) {
        if (module != VK_NULL_HANDLE) vkDestroyShaderModule(device_, module, nullptr);
    }
}

void VulkanRTX::createShaderBindingTable(VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = { 
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR, nullptr, 
        0, 0, 0, 0, 0, 0, 0, 0 
    };
    VkPhysicalDeviceProperties2 properties = { 
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &rtProperties, {} 
    };
    vkGetPhysicalDeviceProperties2(physicalDevice, &properties);

    uint32_t numRaygen = 1, numMiss = 1, numHit = 1 + (hasShaderFeature(ShaderFeatures::Intersection) ? 1 : 0), 
             numCallable = hasShaderFeature(ShaderFeatures::Callable) ? 1 : 0;
    uint32_t groupCount = numRaygen + numMiss + numHit + numCallable;

    const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    const uint32_t baseAlignment = rtProperties.shaderGroupBaseAlignment;
    const uint32_t handleSizeAligned = (handleSize + baseAlignment - 1) / baseAlignment * baseAlignment;

    const VkDeviceSize handlesSize = groupCount * handleSize;
    std::vector<uint8_t> handles(handlesSize);
    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(device_, rtPipeline_.get(), 0, groupCount, handlesSize, handles.data()), 
             "Failed to get shader group handles");

    const VkDeviceSize sbtSize = groupCount * handleSizeAligned;
    createBuffer(physicalDevice, sbtSize, 
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                 sbt_.buffer, sbt_.memory);

    void* data;
    VK_CHECK(vkMapMemory(device_, sbt_.memory.get(), 0, sbtSize, 0, &data), "Failed to map SBT memory");
    uint8_t* pData = static_cast<uint8_t*>(data);
    for (uint32_t g = 0; g < groupCount; ++g) {
        std::memcpy(pData + g * handleSizeAligned, handles.data() + g * handleSize, handleSize);
    }
    vkUnmapMemory(device_, sbt_.memory.get());

    VkBufferDeviceAddressInfo bufferInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, sbt_.buffer.get() };
    VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(device_, &bufferInfo);
    if (sbtAddress == 0) throw VulkanRTXException("Failed to get SBT device address");

    uint32_t raygenStart = 0, missStart = raygenStart + numRaygen, hitStart = missStart + numMiss, 
             callableStart = hitStart + numHit;
    sbt_.raygen = { sbtAddress + raygenStart * handleSizeAligned, numRaygen * handleSizeAligned, handleSizeAligned };
    sbt_.miss = { sbtAddress + missStart * handleSizeAligned, numMiss * handleSizeAligned, handleSizeAligned };
    sbt_.hit = { sbtAddress + hitStart * handleSizeAligned, numHit * handleSizeAligned, handleSizeAligned };
    sbt_.callable = { numCallable ? sbtAddress + callableStart * handleSizeAligned : 0, numCallable * handleSizeAligned, handleSizeAligned };
}

void VulkanRTX::updateDescriptors(VkBuffer_T* cameraBuffer, VkBuffer_T* materialBuffer, VkBuffer_T* dimensionBuffer) {
    std::vector<VkWriteDescriptorSet> writes;
    VkDescriptorBufferInfo cameraInfo = { cameraBuffer, 0, VK_WHOLE_SIZE };
    writes.push_back({
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, ds_.get(), 
        static_cast<uint32_t>(DescriptorBindings::CameraUBO), 0, 1, 
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &cameraInfo, nullptr 
    });
    VkDescriptorBufferInfo materialInfo = { materialBuffer, 0, VK_WHOLE_SIZE };
    writes.push_back({
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, ds_.get(), 
        static_cast<uint32_t>(DescriptorBindings::MaterialSSBO), 0, 1, 
        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &materialInfo, nullptr 
    });
    if (dimensionBuffer != VK_NULL_HANDLE) {
        VkDescriptorBufferInfo dimInfo = { dimensionBuffer, 0, VK_WHOLE_SIZE };
        writes.push_back({
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, ds_.get(), 
            static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO), 0, 1, 
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, nullptr, &dimInfo, nullptr 
        });
    }
    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanRTX::updateDescriptorSetForTLAS(VkAccelerationStructureKHR_T* tlas) {
    VkWriteDescriptorSetAccelerationStructureKHR asInfo = { 
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR, nullptr, 1, &tlas 
    };
    VkWriteDescriptorSet accelWrite = { 
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, &asInfo, ds_.get(), 
        static_cast<uint32_t>(DescriptorBindings::TLAS), 0, 1, 
        VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, nullptr, nullptr, nullptr 
    };
    vkUpdateDescriptorSets(device_, 1, &accelWrite, 0, nullptr);
}

void VulkanRTX::createBuffer(
    VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, 
    VkMemoryPropertyFlags props, VulkanResource<VkBuffer_T*, vkDestroyBuffer>& buffer, 
    VulkanResource<VkDeviceMemory, vkFreeMemory>& memory
) {
    if (size == 0) throw VulkanRTXException("Buffer size must be greater than 0");
    VkBufferCreateInfo bufferInfo = { 
        VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, nullptr, 0, size, usage, 
        VK_SHARING_MODE_EXCLUSIVE, 0, nullptr 
    };
    VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer.get()), "Buffer creation failed");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device_, buffer.get(), &memReqs);
    uint32_t memType = findMemoryType(physicalDevice, memReqs.memoryTypeBits, props);
    VkMemoryAllocateFlagsInfo allocFlagsInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, nullptr,
        static_cast<VkMemoryAllocateFlags>(usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT ? 
                                          VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT : 0), 0
    };
    VkMemoryAllocateInfo allocInfo = {
        VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &allocFlagsInfo : nullptr,
        memReqs.size, memType
    };
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &memory.get()), "Memory allocation failed");
    VK_CHECK(vkBindBufferMemory(device_, buffer.get(), memory.get(), 0), "Buffer memory binding failed");
}

uint32_t VulkanRTX::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    throw VulkanRTXException("No suitable memory type found");
}

VkShaderModule VulkanRTX::createShaderModule(const std::string& filename) {
    std::lock_guard<std::mutex> lock(shaderModuleMutex_);
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw VulkanRTXException("Failed to open shader: " + filename);
    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    VkShaderModuleCreateInfo info = { 
        VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, fileSize, 
        reinterpret_cast<const uint32_t*>(buffer.data()) 
    };
    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(device_, &info, nullptr, &module), 
             "Shader module creation failed for " + filename);
    return module;
}

bool VulkanRTX::shaderFileExists(const std::string& filename) const {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    return file.is_open();
}

void VulkanRTX::loadShadersAsync(std::vector<VkShaderModule>& modules, const std::vector<std::string>& paths) {
    std::vector<std::future<VkShaderModule>> futures;
    for (const auto& path : paths) {
        futures.emplace_back(std::async(std::launch::async, [this, path]() {
            return shaderFileExists(path) ? createShaderModule(path) : VK_NULL_HANDLE;
        }));
    }
    for (size_t i = 0; i < futures.size(); ++i) {
        modules[i] = futures[i].get();
        if (i < 3 && modules[i] == VK_NULL_HANDLE) {
            throw VulkanRTXException("Failed to load required shader: " + paths[i]);
        }
    }
}

void VulkanRTX::buildShaderGroups(std::vector<VkRayTracingShaderGroupCreateInfoKHR>& groups, 
                                  const std::vector<VkPipelineShaderStageCreateInfo>& stages) {
    groups = {
        { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, 
          VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0, VK_SHADER_UNUSED_KHR, 
          VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr },
        { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, 
          VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1, VK_SHADER_UNUSED_KHR, 
          VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr },
        { VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, 
          VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 
          2, hasShaderFeature(ShaderFeatures::AnyHit) ? 3 : VK_SHADER_UNUSED_KHR, 
          VK_SHADER_UNUSED_KHR, nullptr }
    };
    if (hasShaderFeature(ShaderFeatures::Intersection)) {
        groups.push_back({ 
            VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, 
            VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
            VK_SHADER_UNUSED_KHR, 2, hasShaderFeature(ShaderFeatures::AnyHit) ? 3 : VK_SHADER_UNUSED_KHR, 
            stages.size() > 4 ? 4 : VK_SHADER_UNUSED_KHR, nullptr 
        });
    }
    if (hasShaderFeature(ShaderFeatures::Callable)) {
        groups.push_back({ 
            VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, 
            VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 
            static_cast<uint32_t>(stages.size()) - 1, VK_SHADER_UNUSED_KHR, 
            VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, nullptr 
        });
    }
}

void VulkanRTX::createStorageImage(
    VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat format,
    VkImage& image, VkImageView& imageView, VkDeviceMemory& memory
) {
    VkImageCreateInfo imageInfo = {
        VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO, nullptr, 0, VK_IMAGE_TYPE_2D, format,
        {extent.width, extent.height, 1}, 1, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | 
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VK_SHARING_MODE_EXCLUSIVE, 0, nullptr, VK_IMAGE_LAYOUT_UNDEFINED
    };
    VK_CHECK(vkCreateImage(device_, &imageInfo, nullptr, &image), "Failed to create storage image");

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device_, image, &memReqs);
    uint32_t memType = findMemoryType(physicalDevice, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkMemoryAllocateInfo allocInfo = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, nullptr, memReqs.size, memType };
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &memory), "Failed to allocate storage image memory");
    VK_CHECK(vkBindImageMemory(device_, image, memory, 0), "Failed to bind storage image memory");

    VkImageViewCreateInfo viewInfo = {
        VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr, 0, image, VK_IMAGE_VIEW_TYPE_2D, format,
        {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &imageView), "Failed to create storage image view");
}

void VulkanRTX::recordRayTracingCommands(
    VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage, VkImageView outputImageView,
    const PushConstants& pc, VkAccelerationStructureKHR_T* tlas
) {
    if (tlas == VK_NULL_HANDLE) tlas = tlas_.get();
    if (tlas != tlas_.get()) updateDescriptorSetForTLAS(tlas);

    VkDescriptorImageInfo imageInfo = { VK_NULL_HANDLE, outputImageView, VK_IMAGE_LAYOUT_GENERAL };
    VkWriteDescriptorSet imageWrite = { 
        VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, ds_.get(), 
        static_cast<uint32_t>(DescriptorBindings::StorageImage), 0, 1, 
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &imageInfo, nullptr, nullptr 
    };
    vkUpdateDescriptorSets(device_, 1, &imageWrite, 0, nullptr);

    VkImageMemoryBarrier barrier = {
        VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, nullptr, 0, VK_ACCESS_SHADER_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED,
        outputImage, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_.get());
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout_.get(), 
                            0, 1, &ds_.get(), 0, nullptr);
    vkCmdPushConstants(cmdBuffer, rtPipelineLayout_.get(), 
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | 
                       VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR | 
                       VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR, 
                       0, sizeof(PushConstants), &pc);
    vkCmdTraceRaysKHR(cmdBuffer, &sbt_.raygen, &sbt_.miss, &sbt_.hit, &sbt_.callable, 
                      extent.width, extent.height, 1);

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VulkanRTX::denoiseImage(VkCommandBuffer /*cmdBuffer*/, VkImage /*inputImage*/, VkImage /*outputImage*/) {
    spdlog::warn("Denoising not implemented; requires external library or Vulkan extension");
}

// Definitions for VulkanASResource methods
VulkanASResource::~VulkanASResource() {
    if (handle_ != VK_NULL_HANDLE && VulkanRTX::vkDestroyASFunc) {
        VulkanRTX::vkDestroyASFunc(device_, handle_, nullptr);
    }
}

VulkanASResource& VulkanASResource::operator=(VulkanASResource&& other) noexcept {
    if (this != &other) {
        if (handle_ != VK_NULL_HANDLE && VulkanRTX::vkDestroyASFunc) {
            VulkanRTX::vkDestroyASFunc(device_, handle_, nullptr);
        }
        device_ = other.device_;
        handle_ = other.handle_;
        other.handle_ = VK_NULL_HANDLE;
    }
    return *this;
}