// VulkanRTX_Setup.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Implements Vulkan ray-tracing functionality using KHR extensions.
// Dependencies: Vulkan 1.3+, VK_KHR_acceleration_structure, VK_KHR_ray_tracing_pipeline, C++20 standard library, logging.hpp.
// Supported platforms: Windows, Linux (AMD, NVIDIA, Intel GPUs only).
// Zachary Geurts 2025

#include "engine/logging.hpp"
#include "engine/Vulkan/Vulkan_RTX.hpp"
#include <future>
#include <thread>
#include <algorithm>
#include <ranges>
#include <cstring>
#include <source_location>
#include <string>
#include <fstream>

// Define VK_CHECK if not already defined, converting VkResult to int for formatting
#ifndef VK_CHECK
#define VK_CHECK(result, msg) do { if ((result) != VK_SUCCESS) { LOG_ERROR_CAT("Vulkan", "{} (VkResult: {})", std::source_location::current(), (msg), static_cast<int>(result)); throw VulkanRTXException((msg)); } } while (0)
#endif

namespace VulkanRTX {

std::atomic<bool> VulkanRTX::functionPtrInitialized_{false};
std::atomic<bool> VulkanRTX::shaderModuleInitialized_{false};
std::mutex VulkanRTX::functionPtrMutex_;
std::mutex VulkanRTX::shaderModuleMutex_;

// ShaderBindingTable implementation
VulkanRTX::ShaderBindingTable::ShaderBindingTable(VkDevice device, VulkanRTX* parent_)
    : parent(parent_),
      buffer(device, VK_NULL_HANDLE, parent_->vkDestroyBuffer),
      memory(device, VK_NULL_HANDLE, parent_->vkFreeMemory) {
    LOG_INFO_CAT("Vulkan", "Created ShaderBindingTable", std::source_location::current());
}

VulkanRTX::ShaderBindingTable::~ShaderBindingTable() {
    LOG_INFO_CAT("Vulkan", "Destroyed ShaderBindingTable", std::source_location::current());
}

VulkanRTX::ShaderBindingTable::ShaderBindingTable(ShaderBindingTable&& other) noexcept
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
    LOG_INFO_CAT("Vulkan", "Moved ShaderBindingTable", std::source_location::current());
}

VulkanRTX::ShaderBindingTable& VulkanRTX::ShaderBindingTable::operator=(ShaderBindingTable&& other) noexcept {
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
        LOG_INFO_CAT("Vulkan", "Move-assigned ShaderBindingTable", std::source_location::current());
    }
    return *this;
}

// Constructor: Initialize Vulkan RTX with thread-safe extension loading
VulkanRTX::VulkanRTX(VkDevice device, const std::vector<std::string>& shaderPaths)
    : device_(device),
      shaderPaths_(shaderPaths),
      dsLayout_(device, VK_NULL_HANDLE, vkDestroyDescriptorSetLayout),
      dsPool_(device, VK_NULL_HANDLE, vkDestroyDescriptorPool),
      ds_(device, VK_NULL_HANDLE, VK_NULL_HANDLE, vkFreeDescriptorSets),
      rtPipelineLayout_(device, VK_NULL_HANDLE, vkDestroyPipelineLayout),
      rtPipeline_(device, VK_NULL_HANDLE, vkDestroyPipeline),
      blasBuffer_(device, VK_NULL_HANDLE, vkDestroyBuffer),
      blasMemory_(device, VK_NULL_HANDLE, vkFreeMemory),
      tlasBuffer_(device, VK_NULL_HANDLE, vkDestroyBuffer),
      tlasMemory_(device, VK_NULL_HANDLE, vkFreeMemory),
      blas_(device, VK_NULL_HANDLE, nullptr),
      tlas_(device, VK_NULL_HANDLE, nullptr),
      extent_{0, 0},
      primitiveCounts_(),
      previousPrimitiveCounts_(),
      previousDimensionCache_(),
      supportsCompaction_(false),
      shaderFeatures_(ShaderFeatures::None),
      sbt_(device, this) {
    if (!device) {
        LOG_ERROR_CAT("Vulkan", "Null Vulkan device provided", std::source_location::current());
        throw VulkanRTXException("Null Vulkan device provided.");
    }

    LOG_INFO_CAT("Vulkan", "Starting VulkanRTX initialization with {} shader paths", std::source_location::current(), shaderPaths.size());

    // Load Vulkan function pointers with atomic flag
    bool expected = false;
    if (functionPtrInitialized_.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        std::lock_guard<std::mutex> lock(functionPtrMutex_);
        vkGetDeviceProcAddrFunc = reinterpret_cast<PFN_vkGetDeviceProcAddr>(dlsym(dlopen(nullptr, RTLD_LAZY), "vkGetDeviceProcAddr"));
        if (!vkGetDeviceProcAddrFunc) {
            LOG_ERROR_CAT("Vulkan", "Failed to load vkGetDeviceProcAddr", std::source_location::current());
            throw VulkanRTXException("Failed to load vkGetDeviceProcAddr.");
        }

        vkGetBufferDeviceAddressFunc = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(vkGetDeviceProcAddrFunc(device_, "vkGetBufferDeviceAddress"));
        vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddrFunc(device_, "vkCmdTraceRaysKHR"));
        vkCreateASFunc = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddrFunc(device_, "vkCreateAccelerationStructureKHR"));
        vkDestroyASFunc = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddrFunc(device_, "vkDestroyAccelerationStructureKHR"));
        vkGetASBuildSizesFunc = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddrFunc(device_, "vkGetAccelerationStructureBuildSizesKHR"));
        vkCmdBuildASFunc = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddrFunc(device_, "vkCmdBuildAccelerationStructuresKHR"));
        vkGetASDeviceAddressFunc = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddrFunc(device_, "vkGetAccelerationStructureDeviceAddressKHR"));
        vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddrFunc(device_, "vkCreateRayTracingPipelinesKHR"));
        vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddrFunc(device_, "vkGetRayTracingShaderGroupHandlesKHR"));
        vkCmdCopyAccelerationStructureKHR = reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(vkGetDeviceProcAddrFunc(device_, "vkCmdCopyAccelerationStructureKHR"));
        vkCreateDescriptorSetLayoutFunc = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(vkGetDeviceProcAddrFunc(device_, "vkCreateDescriptorSetLayout"));
        vkAllocateDescriptorSetsFunc = reinterpret_cast<PFN_vkAllocateDescriptorSets>(vkGetDeviceProcAddrFunc(device_, "vkAllocateDescriptorSets"));
        vkCreateDescriptorPoolFunc = reinterpret_cast<PFN_vkCreateDescriptorPool>(vkGetDeviceProcAddrFunc(device_, "vkCreateDescriptorPool"));
        vkGetPhysicalDeviceProperties2Func = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(vkGetDeviceProcAddrFunc(device_, "vkGetPhysicalDeviceProperties2"));
        vkCreateShaderModuleFunc = reinterpret_cast<PFN_vkCreateShaderModule>(vkGetDeviceProcAddrFunc(device_, "vkCreateShaderModule"));
        vkDestroyDescriptorSetLayout = reinterpret_cast<PFN_vkDestroyDescriptorSetLayout>(vkGetDeviceProcAddrFunc(device_, "vkDestroyDescriptorSetLayout"));
        vkDestroyDescriptorPool = reinterpret_cast<PFN_vkDestroyDescriptorPool>(vkGetDeviceProcAddrFunc(device_, "vkDestroyDescriptorPool"));
        vkFreeDescriptorSets = reinterpret_cast<PFN_vkFreeDescriptorSets>(vkGetDeviceProcAddrFunc(device_, "vkFreeDescriptorSets"));
        vkDestroyPipelineLayout = reinterpret_cast<PFN_vkDestroyPipelineLayout>(vkGetDeviceProcAddrFunc(device_, "vkDestroyPipelineLayout"));
        vkDestroyPipeline = reinterpret_cast<PFN_vkDestroyPipeline>(vkGetDeviceProcAddrFunc(device_, "vkDestroyPipeline"));
        vkDestroyBuffer = reinterpret_cast<PFN_vkDestroyBuffer>(vkGetDeviceProcAddrFunc(device_, "vkDestroyBuffer"));
        vkFreeMemory = reinterpret_cast<PFN_vkFreeMemory>(vkGetDeviceProcAddrFunc(device_, "vkFreeMemory"));
        vkCreateQueryPool = reinterpret_cast<PFN_vkCreateQueryPool>(vkGetDeviceProcAddrFunc(device_, "vkCreateQueryPool"));
        vkDestroyQueryPool = reinterpret_cast<PFN_vkDestroyQueryPool>(vkGetDeviceProcAddrFunc(device_, "vkDestroyQueryPool"));
        vkGetQueryPoolResults = reinterpret_cast<PFN_vkGetQueryPoolResults>(vkGetDeviceProcAddrFunc(device_, "vkGetQueryPoolResults"));
        vkCmdWriteAccelerationStructuresPropertiesKHR = reinterpret_cast<PFN_vkCmdWriteAccelerationStructuresPropertiesKHR>(vkGetDeviceProcAddrFunc(device_, "vkCmdWriteAccelerationStructuresPropertiesKHR"));
        vkCreateBuffer = reinterpret_cast<PFN_vkCreateBuffer>(vkGetDeviceProcAddrFunc(device_, "vkCreateBuffer"));
        vkAllocateMemory = reinterpret_cast<PFN_vkAllocateMemory>(vkGetDeviceProcAddrFunc(device_, "vkAllocateMemory"));
        vkBindBufferMemory = reinterpret_cast<PFN_vkBindBufferMemory>(vkGetDeviceProcAddrFunc(device_, "vkBindBufferMemory"));
        vkGetPhysicalDeviceMemoryProperties = reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(vkGetDeviceProcAddrFunc(device_, "vkGetPhysicalDeviceMemoryProperties"));
        vkBeginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(vkGetDeviceProcAddrFunc(device_, "vkBeginCommandBuffer"));
        vkEndCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(vkGetDeviceProcAddrFunc(device_, "vkEndCommandBuffer"));
        vkAllocateCommandBuffers = reinterpret_cast<PFN_vkAllocateCommandBuffers>(vkGetDeviceProcAddrFunc(device_, "vkAllocateCommandBuffers"));
        vkQueueSubmit = reinterpret_cast<PFN_vkQueueSubmit>(vkGetDeviceProcAddrFunc(device_, "vkQueueSubmit"));
        vkQueueWaitIdle = reinterpret_cast<PFN_vkQueueWaitIdle>(vkGetDeviceProcAddrFunc(device_, "vkQueueWaitIdle"));
        vkFreeCommandBuffers = reinterpret_cast<PFN_vkFreeCommandBuffers>(vkGetDeviceProcAddrFunc(device_, "vkFreeCommandBuffers"));
        vkCmdResetQueryPool = reinterpret_cast<PFN_vkCmdResetQueryPool>(vkGetDeviceProcAddrFunc(device_, "vkCmdResetQueryPool"));
        vkGetBufferMemoryRequirements = reinterpret_cast<PFN_vkGetBufferMemoryRequirements>(vkGetDeviceProcAddrFunc(device_, "vkGetBufferMemoryRequirements"));
        vkMapMemory = reinterpret_cast<PFN_vkMapMemory>(vkGetDeviceProcAddrFunc(device_, "vkMapMemory"));
        vkUnmapMemory = reinterpret_cast<PFN_vkUnmapMemory>(vkGetDeviceProcAddrFunc(device_, "vkUnmapMemory"));
        vkCreateImage = reinterpret_cast<PFN_vkCreateImage>(vkGetDeviceProcAddrFunc(device_, "vkCreateImage"));
        vkDestroyImage = reinterpret_cast<PFN_vkDestroyImage>(vkGetDeviceProcAddrFunc(device_, "vkDestroyImage"));
        vkGetImageMemoryRequirements = reinterpret_cast<PFN_vkGetImageMemoryRequirements>(vkGetDeviceProcAddrFunc(device_, "vkGetImageMemoryRequirements"));
        vkBindImageMemory = reinterpret_cast<PFN_vkBindImageMemory>(vkGetDeviceProcAddrFunc(device_, "vkBindImageMemory"));
        vkCreateImageView = reinterpret_cast<PFN_vkCreateImageView>(vkGetDeviceProcAddrFunc(device_, "vkCreateImageView"));
        vkDestroyImageView = reinterpret_cast<PFN_vkDestroyImageView>(vkGetDeviceProcAddrFunc(device_, "vkDestroyImageView"));
        vkUpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(vkGetDeviceProcAddrFunc(device_, "vkUpdateDescriptorSets"));
        vkCmdPipelineBarrier = reinterpret_cast<PFN_vkCmdPipelineBarrier>(vkGetDeviceProcAddrFunc(device_, "vkCmdPipelineBarrier"));
        vkCmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(vkGetDeviceProcAddrFunc(device_, "vkCmdBindPipeline"));
        vkCmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(vkGetDeviceProcAddrFunc(device_, "vkCmdBindDescriptorSets"));
        vkCmdPushConstants = reinterpret_cast<PFN_vkCmdPushConstants>(vkGetDeviceProcAddrFunc(device_, "vkCmdPushConstants"));
        vkCmdCopyBuffer = reinterpret_cast<PFN_vkCmdCopyBuffer>(vkGetDeviceProcAddrFunc(device_, "vkCmdCopyBuffer"));
        vkCreatePipelineLayout = reinterpret_cast<PFN_vkCreatePipelineLayout>(vkGetDeviceProcAddrFunc(device_, "vkCreatePipelineLayout"));
        vkCreateComputePipelines = reinterpret_cast<PFN_vkCreateComputePipelines>(vkGetDeviceProcAddrFunc(device_, "vkCreateComputePipelines"));
        vkCmdDispatch = reinterpret_cast<PFN_vkCmdDispatch>(vkGetDeviceProcAddrFunc(device_, "vkCmdDispatch"));
        vkDestroyShaderModule = reinterpret_cast<PFN_vkDestroyShaderModule>(vkGetDeviceProcAddrFunc(device_, "vkDestroyShaderModule"));
    }

    if (!vkGetBufferDeviceAddressFunc || !vkCmdTraceRaysKHR || !vkCreateASFunc ||
        !vkDestroyASFunc || !vkGetASBuildSizesFunc || !vkCmdBuildASFunc ||
        !vkGetASDeviceAddressFunc || !vkCreateRayTracingPipelinesKHR ||
        !vkGetRayTracingShaderGroupHandlesKHR) {
        LOG_ERROR_CAT("Vulkan", "Device lacks required ray tracing extensions (Vulkan 1.2+ with VK_KHR_ray_tracing_pipeline)", std::source_location::current());
        throw VulkanRTXException("Device lacks required ray tracing extensions (Vulkan 1.2+ with VK_KHR_ray_tracing_pipeline).");
    }
    setSupportsCompaction(vkCmdCopyAccelerationStructureKHR != nullptr);

    // Set destroy functions for blas_ and tlas_ after loading vkDestroyASFunc
    setBLAS(VK_NULL_HANDLE);
    setTLAS(VK_NULL_HANDLE);

    LOG_INFO_CAT("Vulkan", "VulkanRTX initialized successfully, supportsCompaction={}", std::source_location::current(), getSupportsCompaction());
}

// Create descriptor set layout with all required bindings
void VulkanRTX::createDescriptorSetLayout() {
    LOG_INFO_CAT("Vulkan", "Creating descriptor set layout", std::source_location::current());

    constexpr uint32_t BINDING_COUNT = 6;
    std::array<VkDescriptorSetLayoutBinding, BINDING_COUNT> bindings = {};
    bindings[static_cast<uint32_t>(DescriptorBindings::TLAS)] = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
                      VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        .pImmutableSamplers = nullptr
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::StorageImage)] = {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_COMPUTE_BIT,
        .pImmutableSamplers = nullptr
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::CameraUBO)] = {
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                      VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        .pImmutableSamplers = nullptr
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::MaterialSSBO)] = {
        .binding = 3,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                      VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        .pImmutableSamplers = nullptr
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO)] = {
        .binding = 4,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                      VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        .pImmutableSamplers = nullptr
    };
    bindings[static_cast<uint32_t>(DescriptorBindings::DenoiseImage)] = {
        .binding = 5,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .pImmutableSamplers = nullptr
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = BINDING_COUNT,
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout tempLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &tempLayout), "Descriptor set layout creation failed");
    setDescriptorSetLayout(tempLayout);
    LOG_INFO_CAT("Vulkan", "Created descriptor set layout with {} bindings", std::source_location::current(), BINDING_COUNT);
}

// Create descriptor pool and allocate a single descriptor set
void VulkanRTX::createDescriptorPoolAndSet() {
    LOG_INFO_CAT("Vulkan", "Creating descriptor pool and set", std::source_location::current());

    constexpr uint32_t POOL_SIZE_COUNT = 5;
    std::array<VkDescriptorPoolSize, POOL_SIZE_COUNT> poolSizes = {{
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}
    }};

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1,
        .poolSizeCount = POOL_SIZE_COUNT,
        .pPoolSizes = poolSizes.data()
    };

    VkDescriptorPool tempPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &tempPool), "Descriptor pool creation failed");
    setDescriptorPool(tempPool);

VkDescriptorSetLayout layout = dsLayout_.get();
VkDescriptorSetAllocateInfo allocInfo = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .pNext = nullptr,
    .descriptorPool = getDescriptorPool(),
    .descriptorSetCount = 1,
    .pSetLayouts = &layout
};

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet), "Descriptor set allocation failed");
    setDescriptorSet(descriptorSet);
    LOG_INFO_CAT("Vulkan", "Created descriptor pool and allocated descriptor set", std::source_location::current());
}

// Create ray tracing pipeline with dynamic shader groups
void VulkanRTX::createRayTracingPipeline(uint32_t maxRayRecursionDepth) {
    LOG_INFO_CAT("Vulkan", "Starting ray tracing pipeline creation with max recursion depth={}", std::source_location::current(), maxRayRecursionDepth);

    std::vector<VkShaderModule> shaderModules(shaderPaths_.size(), VK_NULL_HANDLE);
    loadShadersAsync(shaderModules, shaderPaths_);

    setShaderFeatures(ShaderFeatures::None);
    if (shaderModules.size() > 3 && shaderModules[3] != VK_NULL_HANDLE) {
        setShaderFeatures(static_cast<ShaderFeatures>(static_cast<uint32_t>(getShaderFeatures()) | static_cast<uint32_t>(ShaderFeatures::AnyHit)));
        LOG_DEBUG_CAT("Vulkan", "Any-hit shader detected and enabled", std::source_location::current());
    }
    if (shaderModules.size() > 4 && shaderModules[4] != VK_NULL_HANDLE) {
        setShaderFeatures(static_cast<ShaderFeatures>(static_cast<uint32_t>(getShaderFeatures()) | static_cast<uint32_t>(ShaderFeatures::Intersection)));
        LOG_DEBUG_CAT("Vulkan", "Intersection shader detected and enabled for voxel procedural geometry", std::source_location::current());
    }
    if (shaderModules.size() > 5 && shaderModules[5] != VK_NULL_HANDLE) {
        setShaderFeatures(static_cast<ShaderFeatures>(static_cast<uint32_t>(getShaderFeatures()) | static_cast<uint32_t>(ShaderFeatures::Callable)));
        LOG_DEBUG_CAT("Vulkan", "Callable shader detected and enabled", std::source_location::current());
    }

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .module = shaderModules[0],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = shaderModules[1],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .module = shaderModules[2],
            .pName = "main",
            .pSpecializationInfo = nullptr
        }
    };

    if (hasShaderFeature(ShaderFeatures::AnyHit)) {
        VkPipelineShaderStageCreateInfo stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
            .module = shaderModules[3],
            .pName = "main",
            .pSpecializationInfo = nullptr
        };
        stages.push_back(stage);
    }
    if (hasShaderFeature(ShaderFeatures::Intersection)) {
        VkPipelineShaderStageCreateInfo stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR,
            .module = shaderModules[4],
            .pName = "main",
            .pSpecializationInfo = nullptr
        };
        stages.push_back(stage);
    }
    if (hasShaderFeature(ShaderFeatures::Callable)) {
        VkPipelineShaderStageCreateInfo stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_CALLABLE_BIT_KHR,
            .module = shaderModules[5],
            .pName = "main",
            .pSpecializationInfo = nullptr
        };
        stages.push_back(stage);
    }

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;
    buildShaderGroups(groups, stages);

    VkPushConstantRange pushConstant = {
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                      VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                      VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        .offset = 0,
        .size = sizeof(PushConstants)
    };

VkDescriptorSetLayout layout = dsLayout_.get();
VkPipelineLayoutCreateInfo layoutInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .setLayoutCount = 1,
    .pSetLayouts = &layout,
    .pushConstantRangeCount = 1,
    .pPushConstantRanges = &pushConstant
};

    VkPipelineLayout tempLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &tempLayout), "Ray tracing pipeline layout creation failed");
    setPipelineLayout(tempLayout);
    LOG_INFO_CAT("Vulkan", "Created pipeline layout", std::source_location::current());

    VkRayTracingPipelineCreateInfoKHR createInfo = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = static_cast<uint32_t>(groups.size()),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = maxRayRecursionDepth,
        .pLibraryInfo = nullptr,
        .pLibraryInterface = nullptr,
        .pDynamicState = nullptr,
        .layout = getPipelineLayout(),
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VkPipeline tempPipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &createInfo, nullptr, &tempPipeline),
             "Ray tracing pipeline creation failed");
    setPipeline(tempPipeline);

    for (auto module : shaderModules) {
        if (module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, module, nullptr);
        }
    }
    LOG_INFO_CAT("Vulkan", "Created ray tracing pipeline with {} stages and {} groups", std::source_location::current(), stages.size(), groups.size());
}

// Create shader binding table with aligned handles
void VulkanRTX::createShaderBindingTable(VkPhysicalDevice physicalDevice) {
    LOG_INFO_CAT("Vulkan", "Starting shader binding table creation", std::source_location::current());

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
        .pNext = nullptr,
        .shaderGroupHandleSize = 0,
        .maxRayRecursionDepth = 0,
        .maxShaderGroupStride = 0,
        .shaderGroupBaseAlignment = 0,
        .shaderGroupHandleCaptureReplaySize = 0,
        .maxRayDispatchInvocationCount = 0,
        .shaderGroupHandleAlignment = 0,
        .maxRayHitAttributeSize = 0
    };
    VkPhysicalDeviceProperties2 properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProperties,
        .properties = {}
    };
    vkGetPhysicalDeviceProperties2Func(physicalDevice, &properties);

    constexpr uint32_t NUM_RAYGEN = 1, NUM_MISS = 1, NUM_HIT_BASE = 1;
    uint32_t numHit = NUM_HIT_BASE + (hasShaderFeature(ShaderFeatures::Intersection) ? 1 : 0);
    uint32_t numCallable = hasShaderFeature(ShaderFeatures::Callable) ? 1 : 0;
    uint32_t groupCount = NUM_RAYGEN + NUM_MISS + numHit + numCallable;

    const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    const uint32_t baseAlignment = rtProperties.shaderGroupBaseAlignment;
    const uint32_t handleSizeAligned = ((handleSize + baseAlignment - 1) / baseAlignment) * baseAlignment;
    const VkDeviceSize sbtSize = static_cast<VkDeviceSize>(groupCount) * handleSizeAligned;

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> newBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> newMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sbtSize,
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 newBuffer, newMemory);
    setShaderBindingTable(ShaderBindingTable(device_, this));
    sbt_.buffer = std::move(newBuffer);
    sbt_.memory = std::move(newMemory);

    std::vector<uint8_t> handles(groupCount * handleSize);
    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(device_, getPipeline(), 0, groupCount, groupCount * handleSize, handles.data()),
             "Shader group handles fetch failed");

    void* data;
    VK_CHECK(vkMapMemory(device_, sbt_.memory.get(), 0, sbtSize, 0, &data), "SBT memory mapping failed");
    uint8_t* pData = static_cast<uint8_t*>(data);
    for (uint32_t g : std::views::iota(0u, groupCount)) {
        std::memcpy(pData + g * handleSizeAligned, handles.data() + g * handleSize, handleSize);
    }
    vkUnmapMemory(device_, sbt_.memory.get());

    VkBufferDeviceAddressInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = sbt_.buffer.get()
    };
    VkDeviceAddress sbtAddress = vkGetBufferDeviceAddressFunc(device_, &bufferInfo);
    if (sbtAddress == 0) {
        LOG_ERROR_CAT("Vulkan", "SBT device address invalid (0)", std::source_location::current());
        throw VulkanRTXException("SBT device address invalid (0).");
    }

    uint32_t raygenStart = 0, missStart = raygenStart + NUM_RAYGEN, hitStart = missStart + NUM_MISS,
             callableStart = hitStart + numHit;
    sbt_.raygen = { sbtAddress + static_cast<VkDeviceSize>(raygenStart) * handleSizeAligned, handleSizeAligned, static_cast<VkDeviceSize>(NUM_RAYGEN) * handleSizeAligned };
    sbt_.miss = { sbtAddress + static_cast<VkDeviceSize>(missStart) * handleSizeAligned, handleSizeAligned, static_cast<VkDeviceSize>(NUM_MISS) * handleSizeAligned };
    sbt_.hit = { sbtAddress + static_cast<VkDeviceSize>(hitStart) * handleSizeAligned, handleSizeAligned, static_cast<VkDeviceSize>(numHit) * handleSizeAligned };
    sbt_.callable = { numCallable ? sbtAddress + static_cast<VkDeviceSize>(callableStart) * handleSizeAligned : 0, handleSizeAligned, static_cast<VkDeviceSize>(numCallable) * handleSizeAligned };
    LOG_INFO_CAT("Vulkan", "Created shader binding table with {} groups", std::source_location::current(), groupCount);
}

// Update descriptors with batched writes
void VulkanRTX::updateDescriptors(VkBuffer cameraBuffer, VkBuffer materialBuffer, VkBuffer dimensionBuffer) {
    LOG_INFO_CAT("Vulkan", "Starting descriptor update", std::source_location::current());

    if (!cameraBuffer || !materialBuffer) {
        LOG_ERROR_CAT("Vulkan", "Null camera or material buffer (required for ray tracing)", std::source_location::current());
        throw VulkanRTXException("Null camera or material buffer (required for ray tracing).");
    }

    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(3);

    VkDescriptorBufferInfo cameraInfo = { cameraBuffer, 0, VK_WHOLE_SIZE };
    VkWriteDescriptorSet cameraWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = getDescriptorSet(),
        .dstBinding = static_cast<uint32_t>(DescriptorBindings::CameraUBO),
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &cameraInfo,
        .pTexelBufferView = nullptr
    };
    writes.push_back(cameraWrite);
    LOG_DEBUG_CAT("Vulkan", "Added camera buffer descriptor update", std::source_location::current());

    VkDescriptorBufferInfo materialInfo = { materialBuffer, 0, VK_WHOLE_SIZE };
    VkWriteDescriptorSet materialWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = getDescriptorSet(),
        .dstBinding = static_cast<uint32_t>(DescriptorBindings::MaterialSSBO),
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &materialInfo,
        .pTexelBufferView = nullptr
    };
    writes.push_back(materialWrite);
    LOG_DEBUG_CAT("Vulkan", "Added material buffer descriptor update", std::source_location::current());

    if (dimensionBuffer) {
        VkDescriptorBufferInfo dimInfo = { dimensionBuffer, 0, VK_WHOLE_SIZE };
        VkWriteDescriptorSet dimWrite = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = getDescriptorSet(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &dimInfo,
            .pTexelBufferView = nullptr
        };
        writes.push_back(dimWrite);
        LOG_DEBUG_CAT("Vulkan", "Added dimension buffer descriptor update for voxel grid", std::source_location::current());
    } else {
        LOG_WARNING_CAT("Vulkan", "Dimension buffer is null, skipping descriptor update", std::source_location::current());
    }

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    LOG_INFO_CAT("Vulkan", "Updated {} descriptors successfully", std::source_location::current(), writes.size());
}

// Create shader module with thread-safe file reading
VkShaderModule VulkanRTX::createShaderModule(const std::string& filename) {
    LOG_INFO_CAT("Vulkan", "Creating shader module from file: {}", std::source_location::current(), filename);

    if (!shaderFileExists(filename)) {
        LOG_ERROR_CAT("Vulkan", "Shader file not found or unreadable: {}", std::source_location::current(), filename);
        throw VulkanRTXException(std::format("Shader file not found or unreadable: {}.", filename));
    }

    std::lock_guard<std::mutex> lock(shaderModuleMutex_);
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) {
        LOG_ERROR_CAT("Vulkan", "Invalid shader file size (must be multiple of 4 bytes): {}", std::source_location::current(), filename);
        throw VulkanRTXException(std::format("Invalid shader file size (must be multiple of 4 bytes): {}.", filename));
    }
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = fileSize,
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
    };

    VkShaderModule module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device_, &info, nullptr, &module), std::format("Shader module creation failed for: {}.", filename));
    LOG_INFO_CAT("Vulkan", "Created shader module for: {}", std::source_location::current(), filename);
    return module;
}

// Check shader file existence
bool VulkanRTX::shaderFileExists(const std::string& filename) const {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    bool exists = file.is_open();
    LOG_DEBUG_CAT("Vulkan", "Checking shader file {}: {}", std::source_location::current(), filename, exists ? "exists" : "does not exist");
    return exists;
}

// Load shaders asynchronously with error handling for required shaders
void VulkanRTX::loadShadersAsync(std::vector<VkShaderModule>& modules, const std::vector<std::string>& paths) {
    LOG_INFO_CAT("Vulkan", "Starting async shader loading for {} shaders", std::source_location::current(), paths.size());

    if (modules.size() != paths.size()) {
        LOG_ERROR_CAT("Vulkan", "Shader modules/paths mismatch: modules={}, paths={}", std::source_location::current(), modules.size(), paths.size());
        throw VulkanRTXException(std::format("Shader modules/paths mismatch: modules={}, paths={}.", modules.size(), paths.size()));
    }

    const size_t numShaders = paths.size();
    const size_t maxThreads = std::min(numShaders, static_cast<size_t>(std::thread::hardware_concurrency()));
    std::vector<std::future<VkShaderModule>> futures;
    futures.reserve(maxThreads);

    size_t processed = 0;
    while (processed < numShaders) {
        size_t batchSize = std::min(maxThreads, numShaders - processed);
        futures.clear();
        #pragma omp parallel
        {
            #pragma omp single
            {
                for (size_t j : std::views::iota(0u, batchSize)) {
                    size_t idx = processed + j;
                    #pragma omp task
                    {
                        futures.emplace_back(std::async(std::launch::async, [this, path = paths[idx]]() -> VkShaderModule {
                            return shaderFileExists(path) ? createShaderModule(path) : VK_NULL_HANDLE;
                        }));
                    }
                }
            }
        }
        for (size_t j : std::views::iota(0u, batchSize)) {
            modules[processed + j] = futures[j].get();
            if ((processed + j) < 3 && modules[processed + j] == VK_NULL_HANDLE) {
                LOG_ERROR_CAT("Vulkan", "Required core shader missing: {}", std::source_location::current(), paths[processed + j]);
                throw VulkanRTXException(std::format("Required core shader missing: {}.", paths[processed + j]));
            }
        }
        processed += batchSize;
    }
    LOG_INFO_CAT("Vulkan", "Loaded {} shaders asynchronously", std::source_location::current(), numShaders);
}

// Build dynamic shader groups based on available features
void VulkanRTX::buildShaderGroups(std::vector<VkRayTracingShaderGroupCreateInfoKHR>& groups,
                                  const std::vector<VkPipelineShaderStageCreateInfo>& stages) {
    LOG_INFO_CAT("Vulkan", "Building shader groups for {} stages", std::source_location::current(), stages.size());

    groups.clear();
    groups.reserve(stages.size());

    VkRayTracingShaderGroupCreateInfoKHR raygenGroup = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .pNext = nullptr,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = 0,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
        .pShaderGroupCaptureReplayHandle = nullptr
    };
    groups.push_back(raygenGroup);
    LOG_DEBUG_CAT("Vulkan", "Added raygen shader group", std::source_location::current());

    VkRayTracingShaderGroupCreateInfoKHR missGroup = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .pNext = nullptr,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = 1,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
        .pShaderGroupCaptureReplayHandle = nullptr
    };
    groups.push_back(missGroup);
    LOG_DEBUG_CAT("Vulkan", "Added miss shader group", std::source_location::current());

    VkRayTracingShaderGroupCreateInfoKHR hitGroup = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .pNext = nullptr,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = 2,
        .anyHitShader = hasShaderFeature(ShaderFeatures::AnyHit) ? 3 : VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
        .pShaderGroupCaptureReplayHandle = nullptr
    };
    groups.push_back(hitGroup);
    LOG_DEBUG_CAT("Vulkan", "Added triangle hit group{}", std::source_location::current(), hasShaderFeature(ShaderFeatures::AnyHit) ? " with any-hit shader" : "");

    if (hasShaderFeature(ShaderFeatures::Intersection)) {
        VkRayTracingShaderGroupCreateInfoKHR procGroup = {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
            .generalShader = VK_SHADER_UNUSED_KHR,
            .closestHitShader = 2,
            .anyHitShader = hasShaderFeature(ShaderFeatures::AnyHit) ? 3 : VK_SHADER_UNUSED_KHR,
            .intersectionShader = 4,
            .pShaderGroupCaptureReplayHandle = nullptr
        };
        groups.push_back(procGroup);
        LOG_DEBUG_CAT("Vulkan", "Added procedural hit group for voxel intersection shader", std::source_location::current());
    }

    if (hasShaderFeature(ShaderFeatures::Callable)) {
        VkRayTracingShaderGroupCreateInfoKHR callableGroup = {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = static_cast<uint32_t>(stages.size()) - 1,
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        };
        groups.push_back(callableGroup);
        LOG_DEBUG_CAT("Vulkan", "Added callable shader group", std::source_location::current());
    }

    LOG_INFO_CAT("Vulkan", "Built {} shader groups successfully", std::source_location::current(), groups.size());
}

} // namespace VulkanRTX