// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan RTX initialization and shader management for the AMOURANTH RTX Engine.
// Handles Vulkan device setup, ray tracing pipeline creation, shader binding table (SBT), and descriptor management for voxel rendering.
// Supports asynchronous shader loading and optional intersection shaders for procedural voxel geometry.
// Optimized for single-threaded operation with no mutexes, using C++20 features and robust logging.

// How this ties into the voxel world:
// The VulkanRTX class manages Vulkan ray tracing resources to render voxel-based scenes, where each voxel is represented as a cube with 12 triangles (8 vertices, 36 indices).
// DimensionData (grid dimensions, voxel size) is stored in a storage buffer for shader access, enabling ray-voxel intersection tests or procedural geometry.
// The ray tracing pipeline supports optional intersection shaders for procedural voxels, and the top-level acceleration structure (TLAS) handles a single static voxel grid instance.
// Logging mirrors the style of universal_equation.cpp, with thread-safe output and detailed error handling.

#include "engine/Vulkan/Vulkan_RTX.hpp"
#include <fstream>
#include <cstring>
#include <array>
#include <future>
#include <thread>
#include <algorithm>
#include <ranges>
#include <format>
#include <syncstream>
#include <iostream>

// ANSI color codes
#define RESET "\033[0m"
#define MAGENTA "\033[1;35m" // Bold magenta for errors
#define CYAN "\033[1;36m"    // Bold cyan for debug
#define YELLOW "\033[1;33m"  // Bold yellow for warnings
#define GREEN "\033[1;36m"   // Bold green for info
#define BOLD "\033[1m"

// ShaderBindingTable implementation
VulkanRTX::ShaderBindingTable::ShaderBindingTable(VkDevice device, VulkanRTX* parent_)
    : parent(parent_),
      buffer(device, VK_NULL_HANDLE, vkDestroyBuffer),
      memory(device, VK_NULL_HANDLE, vkFreeMemory) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Initializing ShaderBindingTable" << RESET << std::endl;
}

VulkanRTX::ShaderBindingTable::~ShaderBindingTable() {
    std::osyncstream(std::cout) << GREEN << "[INFO] Destroying ShaderBindingTable" << RESET << std::endl;
}

VulkanRTX::ShaderBindingTable::ShaderBindingTable(VulkanRTX::ShaderBindingTable&& other) noexcept
    : raygen(other.raygen),
      miss(other.miss),
      hit(other.hit),
      callable(other.callable),
      parent(other.parent),
      buffer(std::move(other.buffer)),
      memory(std::move(other.memory)) {
    other.raygen = {};
    other.miss = {};
    other.hit = {};
    other.callable = {};
    other.parent = nullptr;
    std::osyncstream(std::cout) << GREEN << "[INFO] Moved ShaderBindingTable" << RESET << std::endl;
}

VulkanRTX::ShaderBindingTable& VulkanRTX::ShaderBindingTable::operator=(VulkanRTX::ShaderBindingTable&& other) noexcept {
    if (this != &other) {
        raygen = other.raygen;
        miss = other.miss;
        hit = other.hit;
        callable = other.callable;
        parent = other.parent;
        buffer = std::move(other.buffer);
        memory = std::move(other.memory);
        other.raygen = {};
        other.miss = {};
        other.hit = {};
        other.callable = {};
        other.parent = nullptr;
        std::osyncstream(std::cout) << GREEN << "[INFO] Move-assigned ShaderBindingTable" << RESET << std::endl;
    }
    return *this;
}

// Constructor: Initialize Vulkan RTX with extension loading
VulkanRTX::VulkanRTX(VkDevice device, const std::vector<std::string>& shaderPaths)
    : device_(device),
      shaderPaths_(shaderPaths),
      dsLayout_(device, VK_NULL_HANDLE, vkDestroyDescriptorSetLayout),
      dsPool_(device, VK_NULL_HANDLE, vkDestroyDescriptorPool),
      ds_(device, VK_NULL_HANDLE, VK_NULL_HANDLE),
      rtPipelineLayout_(device, VK_NULL_HANDLE, vkDestroyPipelineLayout),
      rtPipeline_(device, VK_NULL_HANDLE, vkDestroyPipeline),
      blasBuffer_(device, VK_NULL_HANDLE, vkDestroyBuffer),
      blasMemory_(device, VK_NULL_HANDLE, vkFreeMemory),
      tlasBuffer_(device, VK_NULL_HANDLE, vkDestroyBuffer),
      tlasMemory_(device, VK_NULL_HANDLE, vkFreeMemory),
      sbt_(device, this),
      supportsCompaction_(false),
      shaderFeatures_(ShaderFeatures::None),
      primitiveCounts_(),
      previousPrimitiveCounts_(),
      previousDimensionCache_(),
      blas_(device, VK_NULL_HANDLE, vkDestroyASFunc),
      tlas_(device, VK_NULL_HANDLE, vkDestroyASFunc) {
    if (!device) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Null Vulkan device provided" << RESET << std::endl;
        throw VulkanRTXException("Null Vulkan device provided.");
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Starting VulkanRTX initialization with " << shaderPaths.size() << " shader paths" << RESET << std::endl;

    // Assume single-threaded initialization; no mutex needed
    vkGetBufferDeviceAddressFunc = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(
        vkGetDeviceProcAddr(device_, "vkGetBufferDeviceAddress"));
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddr(device_, "vkCmdTraceRaysKHR"));
    vkCreateASFunc = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(device_, "vkCreateAccelerationStructureKHR"));
    vkDestroyASFunc = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(device_, "vkDestroyAccelerationStructureKHR"));
    vkGetASBuildSizesFunc = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
        vkGetDeviceProcAddr(device_, "vkGetAccelerationStructureBuildSizesKHR"));
    vkCmdBuildASFunc = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(
        vkGetDeviceProcAddr(device_, "vkCmdBuildAccelerationStructuresKHR"));
    vkGetASDeviceAddressFunc = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(
        vkGetDeviceProcAddr(device_, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddr(device_, "vkCreateRayTracingPipelinesKHR"));
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(device_, "vkGetRayTracingShaderGroupHandlesKHR"));
    vkCmdCopyAccelerationStructureKHR = reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(device_, "vkCmdCopyAccelerationStructureKHR"));

    if (!vkGetBufferDeviceAddressFunc || !vkCmdTraceRaysKHR || !vkCreateASFunc ||
        !vkDestroyASFunc || !vkGetASBuildSizesFunc || !vkCmdBuildASFunc ||
        !vkGetASDeviceAddressFunc || !vkCreateRayTracingPipelinesKHR ||
        !vkGetRayTracingShaderGroupHandlesKHR) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Device lacks required ray tracing extensions" << RESET << std::endl;
        throw VulkanRTXException("Device lacks required ray tracing extensions (Vulkan 1.2+ with VK_KHR_ray_tracing_pipeline).");
    }
    supportsCompaction_ = (vkCmdCopyAccelerationStructureKHR != nullptr);
    std::osyncstream(std::cout) << GREEN << "[INFO] VulkanRTX initialized successfully, supportsCompaction=" << supportsCompaction_ << RESET << std::endl;
}

// Create descriptor set layout with all required bindings
void VulkanRTX::createDescriptorSetLayout() {
    constexpr uint32_t BINDING_COUNT = 6;
    std::array<VkDescriptorSetLayoutBinding, BINDING_COUNT> bindings{
        {
            {
                .binding = static_cast<uint32_t>(DescriptorBindings::TLAS),
                .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                              VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR
            },
            {
                .binding = static_cast<uint32_t>(DescriptorBindings::StorageImage),
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT
            },
            {
                .binding = static_cast<uint32_t>(DescriptorBindings::CameraUBO),
                .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                              VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR
            },
            {
                .binding = static_cast<uint32_t>(DescriptorBindings::MaterialSSBO),
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                              VK_SHADER_STAGE_CALLABLE_BIT_KHR
            },
            {
                .binding = static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO),
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                              VK_SHADER_STAGE_CALLABLE_BIT_KHR
            },
            {
                .binding = static_cast<uint32_t>(DescriptorBindings::DenoiseImage),
                .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
            }
        }
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = BINDING_COUNT,
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout tempLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &tempLayout), "Descriptor set layout creation failed");
    dsLayout_ = VulkanResource<VkDescriptorSetLayout, PFN_vkDestroyDescriptorSetLayout>(device_, tempLayout, vkDestroyDescriptorSetLayout);
    std::osyncstream(std::cout) << GREEN << "[INFO] Created descriptor set layout with " << BINDING_COUNT << " bindings" << RESET << std::endl;
}

// Create descriptor pool and allocate a single descriptor set
void VulkanRTX::createDescriptorPoolAndSet() {
    constexpr uint32_t POOL_SIZE_COUNT = 5;
    std::array<VkDescriptorPoolSize, POOL_SIZE_COUNT> poolSizes{
        {
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}
        }
    };

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1,
        .poolSizeCount = POOL_SIZE_COUNT,
        .pPoolSizes = poolSizes.data()
    };

    VkDescriptorPool tempPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &tempPool), "Descriptor pool creation failed");
    dsPool_ = VulkanResource<VkDescriptorPool, PFN_vkDestroyDescriptorPool>(device_, tempPool, vkDestroyDescriptorPool);

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = dsPool_.get(),
        .descriptorSetCount = 1,
        .pSetLayouts = dsLayout_.getPtr()
    };

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet), "Descriptor set allocation failed");
    ds_ = VulkanDescriptorSet(device_, dsPool_.get(), descriptorSet);
    std::osyncstream(std::cout) << GREEN << "[INFO] Created descriptor pool and allocated descriptor set" << RESET << std::endl;
}

// Create ray tracing pipeline with dynamic shader groups
void VulkanRTX::createRayTracingPipeline(uint32_t maxRayRecursionDepth) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Starting ray tracing pipeline creation with max recursion depth=" << maxRayRecursionDepth << RESET << std::endl;

    std::vector<VkShaderModule> shaderModules(shaderPaths_.size(), VK_NULL_HANDLE);
    loadShadersAsync(shaderModules, shaderPaths_);

    shaderFeatures_ = ShaderFeatures::None;
    if (shaderModules.size() > 3 && shaderModules[3] != VK_NULL_HANDLE) {
        shaderFeatures_ = static_cast<ShaderFeatures>(static_cast<uint32_t>(shaderFeatures_) | static_cast<uint32_t>(ShaderFeatures::AnyHit));
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Any-hit shader detected and enabled" << RESET << std::endl;
    }
    if (shaderModules.size() > 4 && shaderModules[4] != VK_NULL_HANDLE) {
        shaderFeatures_ = static_cast<ShaderFeatures>(static_cast<uint32_t>(shaderFeatures_) | static_cast<uint32_t>(ShaderFeatures::Intersection));
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Intersection shader detected and enabled for voxel procedural geometry" << RESET << std::endl;
    }
    if (shaderModules.size() > 5 && shaderModules[5] != VK_NULL_HANDLE) {
        shaderFeatures_ = static_cast<ShaderFeatures>(static_cast<uint32_t>(shaderFeatures_) | static_cast<uint32_t>(ShaderFeatures::Callable));
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Callable shader detected and enabled" << RESET << std::endl;
    }

    std::vector<VkPipelineShaderStageCreateInfo> stages{
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .module = shaderModules[0],
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = shaderModules[1],
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .module = shaderModules[2],
            .pName = "main"
        }
    };

    if (hasShaderFeature(ShaderFeatures::AnyHit)) {
        stages.push_back({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            .module = shaderModules[3],
            .pName = "main"
        });
    }
    if (hasShaderFeature(ShaderFeatures::Intersection)) {
        stages.push_back({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
            .module = shaderModules[4],
            .pName = "main"
        });
    }
    if (hasShaderFeature(ShaderFeatures::Callable)) {
        stages.push_back({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_CALLABLE_BIT_KHR,
            .module = shaderModules[5],
            .pName = "main"
        });
    }

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    buildShaderGroups(groups, stages);

    VkPushConstantRange pushConstant{
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                      VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                      VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        .offset = 0,
        .size = sizeof(PushConstants)
    };

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = dsLayout_.getPtr(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstant
    };

    VkPipelineLayout tempLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &tempLayout), "Ray tracing pipeline layout creation failed");
    rtPipelineLayout_ = VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>(device_, tempLayout, vkDestroyPipelineLayout);

    VkRayTracingPipelineCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = static_cast<uint32_t>(groups.size()),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = maxRayRecursionDepth,
        .layout = rtPipelineLayout_.get()
    };

    VkPipeline tempPipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &createInfo, nullptr, &tempPipeline),
             "Ray tracing pipeline creation failed");
    rtPipeline_ = VulkanResource<VkPipeline, PFN_vkDestroyPipeline>(device_, tempPipeline, vkDestroyPipeline);

    for (auto module : shaderModules) {
        if (module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, module, nullptr);
        }
    }
    std::osyncstream(std::cout) << GREEN << "[INFO] Created ray tracing pipeline with " << stages.size() << " stages and " << groups.size() << " groups" << RESET << std::endl;
}

// Create shader binding table with aligned handles
void VulkanRTX::createShaderBindingTable(VkPhysicalDevice physicalDevice) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Starting shader binding table creation" << RESET << std::endl;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceProperties2 properties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProperties
    };
    vkGetPhysicalDeviceProperties2(physicalDevice, &properties);

    constexpr uint32_t NUM_RAYGEN = 1, NUM_MISS = 1, NUM_HIT_BASE = 1;
    uint32_t numHit = NUM_HIT_BASE + (hasShaderFeature(ShaderFeatures::Intersection) ? 1 : 0);
    uint32_t numCallable = hasShaderFeature(ShaderFeatures::Callable) ? 1 : 0;
    uint32_t groupCount = NUM_RAYGEN + NUM_MISS + numHit + numCallable;

    const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    const uint32_t baseAlignment = rtProperties.shaderGroupBaseAlignment;
    const uint32_t handleSizeAligned = ((handleSize + baseAlignment - 1) / baseAlignment) * baseAlignment;
    const VkDeviceSize sbtSize = static_cast<VkDeviceSize>(groupCount) * handleSizeAligned;

    createBuffer(physicalDevice, sbtSize,
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 sbt_.buffer, sbt_.memory);

    std::vector<uint8_t> handles(groupCount * handleSize);
    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(device_, rtPipeline_.get(), 0, groupCount, groupCount * handleSize, handles.data()),
             "Shader group handles fetch failed");

    void* data;
    VK_CHECK(vkMapMemory(device_, sbt_.memory.get(), 0, sbtSize, 0, &data), "SBT memory mapping failed");
    uint8_t* pData = static_cast<uint8_t*>(data);
    for (uint32_t g : std::views::iota(0u, groupCount)) {
        std::memcpy(pData + g * handleSizeAligned, handles.data() + g * handleSize, handleSize);
    }
    vkUnmapMemory(device_, sbt_.memory.get());

    VkBufferDeviceAddressInfo bufferInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = sbt_.buffer.get() };
    VkDeviceAddress sbtAddress = vkGetBufferDeviceAddressFunc(device_, &bufferInfo);
    if (sbtAddress == 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] SBT device address invalid (0)" << RESET << std::endl;
        throw VulkanRTXException("SBT device address invalid (0).");
    }

    uint32_t raygenStart = 0, missStart = raygenStart + NUM_RAYGEN, hitStart = missStart + NUM_MISS,
             callableStart = hitStart + numHit;
    sbt_.raygen = { sbtAddress + static_cast<VkDeviceSize>(raygenStart) * handleSizeAligned, handleSizeAligned, static_cast<VkDeviceSize>(NUM_RAYGEN) * handleSizeAligned };
    sbt_.miss = { sbtAddress + static_cast<VkDeviceSize>(missStart) * handleSizeAligned, handleSizeAligned, static_cast<VkDeviceSize>(NUM_MISS) * handleSizeAligned };
    sbt_.hit = { sbtAddress + static_cast<VkDeviceSize>(hitStart) * handleSizeAligned, handleSizeAligned, static_cast<VkDeviceSize>(numHit) * handleSizeAligned };
    sbt_.callable = { numCallable ? sbtAddress + static_cast<VkDeviceSize>(callableStart) * handleSizeAligned : 0, handleSizeAligned, static_cast<VkDeviceSize>(numCallable) * handleSizeAligned };
    std::osyncstream(std::cout) << GREEN << "[INFO] Created shader binding table with " << groupCount << " groups" << RESET << std::endl;
}

// Update descriptors with batched writes
void VulkanRTX::updateDescriptors(VkBuffer cameraBuffer, VkBuffer materialBuffer, VkBuffer dimensionBuffer) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Starting descriptor update" << RESET << std::endl;

    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(3);

    if (cameraBuffer) {
        VkDescriptorBufferInfo cameraInfo{ cameraBuffer, 0, VK_WHOLE_SIZE };
        writes.push_back({
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::CameraUBO),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &cameraInfo
        });
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Added camera buffer descriptor update" << RESET << std::endl;
    } else {
        std::osyncstream(std::cout) << YELLOW << "[WARNING] Camera buffer is null, skipping descriptor update" << RESET << std::endl;
    }

    if (materialBuffer) {
        VkDescriptorBufferInfo materialInfo{ materialBuffer, 0, VK_WHOLE_SIZE };
        writes.push_back({
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::MaterialSSBO),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &materialInfo
        });
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Added material buffer descriptor update" << RESET << std::endl;
    } else {
        std::osyncstream(std::cout) << YELLOW << "[WARNING] Material buffer is null, skipping descriptor update" << RESET << std::endl;
    }

    if (dimensionBuffer) {
        VkDescriptorBufferInfo dimInfo{ dimensionBuffer, 0, VK_WHOLE_SIZE };
        writes.push_back({
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pBufferInfo = &dimInfo
        });
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Added dimension buffer descriptor update for voxel grid" << RESET << std::endl;
    } else {
        std::osyncstream(std::cout) << YELLOW << "[WARNING] Dimension buffer is null, skipping descriptor update" << RESET << std::endl;
    }

    if (!writes.empty()) {
        vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
        std::osyncstream(std::cout) << GREEN << "[INFO] Updated " << writes.size() << " descriptors successfully" << RESET << std::endl;
    } else {
        std::osyncstream(std::cout) << YELLOW << "[WARNING] No descriptors to update" << RESET << std::endl;
    }
}

// Create shader module with file reading
VkShaderModule VulkanRTX::createShaderModule(const std::string& filename) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Creating shader module from file: " << filename << RESET << std::endl;

    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Shader file not found or unreadable: " << filename << RESET << std::endl;
        throw VulkanRTXException(std::format("Shader file not found or unreadable: {}.", filename));
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Invalid shader file size (must be multiple of 4 bytes): " << filename << RESET << std::endl;
        throw VulkanRTXException(std::format("Invalid shader file size (must be multiple of 4 bytes): {}.", filename));
    }
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
    };

    VkShaderModule module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device_, &info, nullptr, &module), std::format("Shader module creation failed for: {}.", filename));
    std::osyncstream(std::cout) << GREEN << "[INFO] Created shader module for: " << filename << RESET << std::endl;
    return module;
}

// Check shader file existence
bool VulkanRTX::shaderFileExists(const std::string& filename) const {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    bool exists = file.is_open();
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Checking shader file " << filename << ": " << (exists ? "exists" : "does not exist") << RESET << std::endl;
    return exists;
}

// Load shaders asynchronously with error handling for required shaders
void VulkanRTX::loadShadersAsync(std::vector<VkShaderModule>& modules, const std::vector<std::string>& paths) {
    if (modules.size() != paths.size()) {
        std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Shader modules/paths mismatch: modules=" << modules.size() << ", paths=" << paths.size() << RESET << std::endl;
        throw VulkanRTXException(std::format("Shader modules/paths mismatch: modules={}, paths={}.", modules.size(), paths.size()));
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Starting async shader loading for " << paths.size() << " shaders" << RESET << std::endl;

    const size_t numShaders = paths.size();
    const size_t maxThreads = std::min(numShaders, static_cast<size_t>(std::thread::hardware_concurrency()));
    std::vector<std::future<VkShaderModule>> futures;
    futures.reserve(maxThreads);

    size_t processed = 0;
    while (processed < numShaders) {
        size_t batchSize = std::min(maxThreads, numShaders - processed);
        futures.clear();
        for (size_t j : std::views::iota(0u, batchSize)) {
            size_t idx = processed + j;
            futures.emplace_back(std::async(std::launch::async, [this, path = paths[idx]]() -> VkShaderModule {
                return shaderFileExists(path) ? createShaderModule(path) : VK_NULL_HANDLE;
            }));
        }
        for (size_t j : std::views::iota(0u, batchSize)) {
            modules[processed + j] = futures[j].get();
            if ((processed + j) < 3 && modules[processed + j] == VK_NULL_HANDLE) {
                std::osyncstream(std::cerr) << MAGENTA << "[ERROR] Required core shader missing: " << paths[processed + j] << RESET << std::endl;
                throw VulkanRTXException(std::format("Required core shader missing: {}.", paths[processed + j]));
            }
        }
        processed += batchSize;
    }
    std::osyncstream(std::cout) << GREEN << "[INFO] Loaded " << numShaders << " shaders asynchronously" << RESET << std::endl;
}

// Build dynamic shader groups based on available features
void VulkanRTX::buildShaderGroups(std::vector<VkRayTracingShaderGroupCreateInfoKHR>& groups,
                                  const std::vector<VkPipelineShaderStageCreateInfo>& stages) {
    std::osyncstream(std::cout) << GREEN << "[INFO] Building shader groups for " << stages.size() << " stages" << RESET << std::endl;

    groups.clear();
    groups.reserve(stages.size());

    groups.push_back({
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = 0,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    });
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Added raygen shader group" << RESET << std::endl;

    groups.push_back({
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = 1,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    });
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Added miss shader group" << RESET << std::endl;

    groups.push_back({
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = 2,
        .anyHitShader = hasShaderFeature(ShaderFeatures::AnyHit) ? 3 : VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR
    });
    std::osyncstream(std::cout) << CYAN << "[DEBUG] Added triangle hit group" << (hasShaderFeature(ShaderFeatures::AnyHit) ? " with any-hit shader" : "") << RESET << std::endl;

    if (hasShaderFeature(ShaderFeatures::Intersection)) {
        groups.push_back({
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2,
            .anyHitShader = hasShaderFeature(ShaderFeatures::AnyHit) ? 3 : VK_SHADER_UNUSED_KHR,
            .intersectionShader = 4
        });
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Added procedural hit group for voxel intersection shader" << RESET << std::endl;
    }

    if (hasShaderFeature(ShaderFeatures::Callable)) {
        groups.push_back({
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = static_cast<uint32_t>(stages.size()) - 1,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR
        });
        std::osyncstream(std::cout) << CYAN << "[DEBUG] Added callable shader group" << RESET << std::endl;
    }

    std::osyncstream(std::cout) << GREEN << "[INFO] Built " << groups.size() << " shader groups successfully" << RESET << std::endl;
}