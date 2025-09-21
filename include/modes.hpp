#ifndef MODES_HPP
#define MODES_HPP

#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include <vector>
#include <iostream>
#include <vulkan/vulkan.h>
#include "universal_equation.hpp"
#include "types.hpp"

class DimensionalNavigator;

void renderMode1(DimensionalNavigator* navigator, uint32_t imageIndex);
void renderMode2(DimensionalNavigator* navigator, uint32_t imageIndex, VkBuffer vertexBuffer_,
                 std::vector<VkCommandBuffer>& commandBuffers_, VkBuffer indexBuffer_,
                 float zoomLevel_, int width_, int height_, float wavePhase_,
                 std::vector<DimensionData>& cache_);
void renderMode3(DimensionalNavigator* navigator, uint32_t imageIndex, VkBuffer vertexBuffer_,
                 std::vector<VkCommandBuffer>& commandBuffers_, VkBuffer indexBuffer_,
                 float zoomLevel_, int width_, int height_, float wavePhase_,
                 std::vector<DimensionData>& cache_);
void renderMode4(DimensionalNavigator* navigator, uint32_t imageIndex, VkBuffer vertexBuffer_,
                 std::vector<VkCommandBuffer>& commandBuffers_, VkBuffer indexBuffer_,
                 float zoomLevel_, int width_, int height_, float wavePhase_,
                 std::vector<DimensionData>& cache_);
void renderMode5(DimensionalNavigator* navigator, uint32_t imageIndex, VkBuffer vertexBuffer_,
                 std::vector<VkCommandBuffer>& commandBuffers_, VkBuffer indexBuffer_,
                 float zoomLevel_, int width_, int height_, float wavePhase_,
                 std::vector<DimensionData>& cache_);
void renderMode6(DimensionalNavigator* navigator, uint32_t imageIndex, VkBuffer vertexBuffer_,
                 std::vector<VkCommandBuffer>& commandBuffers_, VkBuffer indexBuffer_,
                 float zoomLevel_, int width_, int height_, float wavePhase_,
                 std::vector<DimensionData>& cache_);
void renderMode7(DimensionalNavigator* navigator, uint32_t imageIndex, VkBuffer vertexBuffer_,
                 std::vector<VkCommandBuffer>& commandBuffers_, VkBuffer indexBuffer_,
                 float zoomLevel_, int width_, int height_, float wavePhase_,
                 std::vector<DimensionData>& cache_);
void renderMode8(DimensionalNavigator* navigator, uint32_t imageIndex, VkBuffer vertexBuffer_,
                 std::vector<VkCommandBuffer>& commandBuffers_, VkBuffer indexBuffer_,
                 float zoomLevel_, int width_, int height_, float wavePhase_,
                 std::vector<DimensionData>& cache_);
void renderMode9(DimensionalNavigator* navigator, uint32_t imageIndex, VkBuffer vertexBuffer_,
                 std::vector<VkCommandBuffer>& commandBuffers_, VkBuffer indexBuffer_,
                 float zoomLevel_, int width_, int height_, float wavePhase_,
                 std::vector<DimensionData>& cache_);
#endif // MODES_HPP