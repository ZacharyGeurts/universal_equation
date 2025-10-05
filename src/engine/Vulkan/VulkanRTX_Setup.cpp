// AMOURANTH RTX © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#include "engine/Vulkan/Vulkan_RTX.hpp"
#include <spdlog/spdlog.h>
#include <fstream>
#include <cstring>
#include <array>
#include <future>
#include <thread>
#include <algorithm>
#include <ranges>

// Static mutexes for thread-safe initialization
std::mutex VulkanRTX::functionPtrMutex_;
std::mutex VulkanRTX::shaderModuleMutex_;

// ShaderBindingTable implementation
VulkanRTX::ShaderBindingTable::ShaderBindingTable(VkDevice device, VulkanRTX* parent_)
    : parent(parent_),
      buffer(device, VK_NULL_HANDLE, vkDestroyBuffer),
      memory(device, VK_NULL_HANDLE, vkFreeMemory) {}

VulkanRTX::ShaderBindingTable::~ShaderBindingTable() {}

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
    }
    return *this;
}

// Constructor: Initialize Vulkan RTX with thread-safe extension loading
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
    if (!device) throw VulkanRTXException("Null Vulkan device provided.");

    std::lock_guard<std::mutex> lock(functionPtrMutex_);
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
        throw VulkanRTXException("Device lacks required ray tracing extensions (Vulkan 1.2+ with VK_KHR_ray_tracing_pipeline).");
    }
    supportsCompaction_ = (vkCmdCopyAccelerationStructureKHR != nullptr);
}

// Create descriptor set layout with all required bindings
void VulkanRTX::createDescriptorSetLayout() {
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

    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = BINDING_COUNT;
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout tempLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &tempLayout), "Descriptor set layout creation failed");
    dsLayout_ = VulkanResource<VkDescriptorSetLayout, PFN_vkDestroyDescriptorSetLayout>(device_, tempLayout, vkDestroyDescriptorSetLayout);
}

// Create descriptor pool and allocate a single descriptor set
void VulkanRTX::createDescriptorPoolAndSet() {
    constexpr uint32_t POOL_SIZE_COUNT = 5;
    std::array<VkDescriptorPoolSize, POOL_SIZE_COUNT> poolSizes = {{
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2}
    }};

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = POOL_SIZE_COUNT;
    poolInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool tempPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &tempPool), "Descriptor pool creation failed");
    dsPool_ = VulkanResource<VkDescriptorPool, PFN_vkDestroyDescriptorPool>(device_, tempPool, vkDestroyDescriptorPool);

    VkDescriptorSetAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = dsPool_.get();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = dsLayout_.getPtr();

    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet), "Descriptor set allocation failed");
    ds_ = VulkanDescriptorSet(device_, dsPool_.get(), descriptorSet);
}

// Create ray tracing pipeline with dynamic shader groups
void VulkanRTX::createRayTracingPipeline(uint32_t maxRayRecursionDepth) {
    std::vector<VkShaderModule> shaderModules(shaderPaths_.size(), VK_NULL_HANDLE);
    loadShadersAsync(shaderModules, shaderPaths_);

    shaderFeatures_ = ShaderFeatures::None;
    if (shaderModules.size() > 3 && shaderModules[3] != VK_NULL_HANDLE) {
        shaderFeatures_ = static_cast<ShaderFeatures>(static_cast<uint32_t>(shaderFeatures_) | static_cast<uint32_t>(ShaderFeatures::AnyHit));
    }
    if (shaderModules.size() > 4 && shaderModules[4] != VK_NULL_HANDLE) {
        shaderFeatures_ = static_cast<ShaderFeatures>(static_cast<uint32_t>(shaderFeatures_) | static_cast<uint32_t>(ShaderFeatures::Intersection));
    }
    if (shaderModules.size() > 5 && shaderModules[5] != VK_NULL_HANDLE) {
        shaderFeatures_ = static_cast<ShaderFeatures>(static_cast<uint32_t>(shaderFeatures_) | static_cast<uint32_t>(ShaderFeatures::Callable));
    }

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .module = shaderModules[0],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        VkPipelineShaderStageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = shaderModules[1],
            .pName = "main",
            .pSpecializationInfo = nullptr
        },
        VkPipelineShaderStageCreateInfo{
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

    VkPushConstantRange pushConstant = {};
    pushConstant.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                             VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                             VK_SHADER_STAGE_INTERSECTION_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = dsLayout_.getPtr();
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstant;

    VkPipelineLayout tempLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &tempLayout), "Ray tracing pipeline layout creation failed");
    rtPipelineLayout_ = VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>(device_, tempLayout, vkDestroyPipelineLayout);

    VkRayTracingPipelineCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    createInfo.stageCount = static_cast<uint32_t>(stages.size());
    createInfo.pStages = stages.data();
    createInfo.groupCount = static_cast<uint32_t>(groups.size());
    createInfo.pGroups = groups.data();
    createInfo.maxPipelineRayRecursionDepth = maxRayRecursionDepth;
    createInfo.layout = rtPipelineLayout_.get();

    VkPipeline tempPipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &createInfo, nullptr, &tempPipeline),
             "Ray tracing pipeline creation failed");
    rtPipeline_ = VulkanResource<VkPipeline, PFN_vkDestroyPipeline>(device_, tempPipeline, vkDestroyPipeline);

    for (auto module : shaderModules) {
        if (module != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, module, nullptr);
        }
    }
}

// Create shader binding table with aligned handles
void VulkanRTX::createShaderBindingTable(VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {};
    rtProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &rtProperties;
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

    VkBufferDeviceAddressInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    bufferInfo.buffer = sbt_.buffer.get();
    VkDeviceAddress sbtAddress = vkGetBufferDeviceAddressFunc(device_, &bufferInfo);
    if (sbtAddress == 0) {
        throw VulkanRTXException("SBT device address invalid (0).");
    }

    uint32_t raygenStart = 0, missStart = raygenStart + NUM_RAYGEN, hitStart = missStart + NUM_MISS,
             callableStart = hitStart + numHit;
    sbt_.raygen = {sbtAddress + static_cast<VkDeviceSize>(raygenStart) * handleSizeAligned, handleSizeAligned, static_cast<VkDeviceSize>(NUM_RAYGEN) * handleSizeAligned};
    sbt_.miss = {sbtAddress + static_cast<VkDeviceSize>(missStart) * handleSizeAligned, handleSizeAligned, static_cast<VkDeviceSize>(NUM_MISS) * handleSizeAligned};
    sbt_.hit = {sbtAddress + static_cast<VkDeviceSize>(hitStart) * handleSizeAligned, handleSizeAligned, static_cast<VkDeviceSize>(numHit) * handleSizeAligned};
    sbt_.callable = {numCallable ? sbtAddress + static_cast<VkDeviceSize>(callableStart) * handleSizeAligned : 0, handleSizeAligned, static_cast<VkDeviceSize>(numCallable) * handleSizeAligned};
}

// Update descriptors with batched writes
void VulkanRTX::updateDescriptors(VkBuffer cameraBuffer, VkBuffer materialBuffer, VkBuffer dimensionBuffer) {
    if (!cameraBuffer || !materialBuffer) {
        throw VulkanRTXException("Null camera or material buffer (required for ray tracing).");
    }
    std::vector<VkWriteDescriptorSet> writes;
    writes.reserve(3);

    VkDescriptorBufferInfo cameraInfo = {cameraBuffer, 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSet cameraWrite = {};
    cameraWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    cameraWrite.dstSet = ds_.get();
    cameraWrite.dstBinding = static_cast<uint32_t>(DescriptorBindings::CameraUBO);
    cameraWrite.dstArrayElement = 0;
    cameraWrite.descriptorCount = 1;
    cameraWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    cameraWrite.pBufferInfo = &cameraInfo;
    writes.push_back(cameraWrite);

    VkDescriptorBufferInfo materialInfo = {materialBuffer, 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSet materialWrite = {};
    materialWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    materialWrite.dstSet = ds_.get();
    materialWrite.dstBinding = static_cast<uint32_t>(DescriptorBindings::MaterialSSBO);
    materialWrite.dstArrayElement = 0;
    materialWrite.descriptorCount = 1;
    materialWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    materialWrite.pBufferInfo = &materialInfo;
    writes.push_back(materialWrite);

    if (dimensionBuffer) {
        VkDescriptorBufferInfo dimInfo = {dimensionBuffer, 0, VK_WHOLE_SIZE};
        VkWriteDescriptorSet dimWrite = {};
        dimWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        dimWrite.dstSet = ds_.get();
        dimWrite.dstBinding = static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO);
        dimWrite.dstArrayElement = 0;
        dimWrite.descriptorCount = 1;
        dimWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        dimWrite.pBufferInfo = &dimInfo;
        writes.push_back(dimWrite);
    }

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

// Create shader module with thread-safe file reading
VkShaderModule VulkanRTX::createShaderModule(const std::string& filename) {
    std::lock_guard<std::mutex> lock(shaderModuleMutex_);
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw VulkanRTXException("Shader file not found or unreadable: " + filename);
    }
    size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) {
        throw VulkanRTXException("Invalid shader file size (must be multiple of 4 bytes): " + filename);
    }
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = fileSize;
    info.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VkShaderModule module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(device_, &info, nullptr, &module), "Shader module creation failed for: " + filename);
    return module;
}

// Check shader file existence
bool VulkanRTX::shaderFileExists(const std::string& filename) const {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    return file.is_open();
}

// Load shaders asynchronously with error handling for required shaders
void VulkanRTX::loadShadersAsync(std::vector<VkShaderModule>& modules, const std::vector<std::string>& paths) {
    if (modules.size() != paths.size()) {
        throw VulkanRTXException("Shader modules/paths mismatch.");
    }
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
                throw VulkanRTXException("Required core shader missing: " + paths[processed + j]);
            }
        }
        processed += batchSize;
    }
}

// Build dynamic shader groups based on available features
void VulkanRTX::buildShaderGroups(std::vector<VkRayTracingShaderGroupCreateInfoKHR>& groups,
                                  const std::vector<VkPipelineShaderStageCreateInfo>& stages) {
    groups.clear();
    groups.reserve(stages.size());

    VkRayTracingShaderGroupCreateInfoKHR raygenGroup = {};
    raygenGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    raygenGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    raygenGroup.generalShader = 0;
    raygenGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    raygenGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    raygenGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    groups.push_back(raygenGroup);

    VkRayTracingShaderGroupCreateInfoKHR missGroup = {};
    missGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    missGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    missGroup.generalShader = 1;
    missGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
    missGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
    missGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    groups.push_back(missGroup);

    VkRayTracingShaderGroupCreateInfoKHR hitGroup = {};
    hitGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    hitGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    hitGroup.generalShader = VK_SHADER_UNUSED_KHR;
    hitGroup.closestHitShader = 2;
    hitGroup.anyHitShader = hasShaderFeature(ShaderFeatures::AnyHit) ? 3 : VK_SHADER_UNUSED_KHR;
    hitGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
    groups.push_back(hitGroup);

    if (hasShaderFeature(ShaderFeatures::Intersection)) {
        VkRayTracingShaderGroupCreateInfoKHR procGroup = {};
        procGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        procGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR;
        procGroup.generalShader = VK_SHADER_UNUSED_KHR;
        procGroup.closestHitShader = 2;
        procGroup.anyHitShader = hasShaderFeature(ShaderFeatures::AnyHit) ? 3 : VK_SHADER_UNUSED_KHR;
        procGroup.intersectionShader = 4;
        groups.push_back(procGroup);
    }

    if (hasShaderFeature(ShaderFeatures::Callable)) {
        VkRayTracingShaderGroupCreateInfoKHR callableGroup = {};
        callableGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        callableGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        callableGroup.generalShader = static_cast<uint32_t>(stages.size()) - 1;
        callableGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
        callableGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
        callableGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
        groups.push_back(callableGroup);
    }
}