// PushConstants.hpp
#ifndef PUSH_CONSTANTS_HPP
#define PUSH_CONSTANTS_HPP

#include <cstdint>

// Push constant structure for UniversalEquation and lighting parameters
struct PushConstants {
    float influence;           // Influence factor (0 to 10)
    float weak;                // Weak interaction strength (0 to 1)
    float collapse;            // Collapse factor (0 to 5)
    float twoD;                // 2D influence factor (0 to 5)
    float threeDInfluence;     // 3D influence factor (0 to 5)
    float oneDPermeation;      // 1D permeation factor (0 to 5)
    float darkMatterStrength;  // Dark matter strength (0 to 1)
    float darkEnergyStrength;  // Dark energy strength (0 to 2)
    float alpha;               // Exponential decay factor (0.1 to 10)
    float beta;                // Permeation scaling factor (0 to 1)
    int32_t currentDimension;  // Current dimension for computation (1 to 20)
    int32_t mode;              // Current mode
    float omega;               // Angular frequency for 2D oscillation
    float invMaxDim;           // Inverse of max dimensions for scaling
    float ambientColor[4];     // Ambient light color and intensity (RGBA)
    float diffuseColor[4];     // Diffuse light color and intensity (RGBA)
    float specularColor[4];    // Specular light color and intensity (RGBA)
    float lightDirection[3];   // Normalized light direction (world space)
    float specularPower;       // Shininess factor for specular highlights
    float ambientIntensity;    // Ambient light intensity multiplier
    float diffuseIntensity;    // Diffuse light intensity multiplier
    float specularIntensity;   // Specular light intensity multiplier
    float padding[2];          // Padding to align to 256 bytes

    // Size calculation:
    // Floats: 10 * 4 = 40 bytes
    // Int32: 2 * 4 = 8 bytes
    // Float arrays (vec4): 3 * 4 * 4 = 48 bytes
    // Float array (vec3): 3 * 4 = 12 bytes
    // Float (specularPower, intensities): 4 * 4 = 16 bytes
    // Padding: 2 * 4 = 8 bytes
    // Total: 40 + 8 + 48 + 12 + 16 + 8 = 132 bytes (< 256 bytes)
};

#endif // PUSH_CONSTANTS_HPP
