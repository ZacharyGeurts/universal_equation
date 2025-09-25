#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_ARB_separate_shader_objects : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

// Common definitions shared across shaders
struct PushConstants {
    mat4 view;          // offset 0 (model not needed in RT; handled by BLAS/TLAS)
    mat4 proj;          // offset 64
    vec3 baseColor;     // offset 128
    float value;        // offset 140
    float dimValue;     // offset 144
    float wavePhase;    // offset 148
    float cycleProgress; // offset 152
    float darkMatter;   // offset 156
    float darkEnergy;   // offset 160
};

// Ray payload for color
layout(location = 0) rayPayloadInEXT vec3 payloadIn;

// Push constants (adjusted offsets since model is omitted)
layout(push_constant) uniform PushConstantsPC {
    PushConstants pc;
} pushConstants;

// =====================================================================
// Miss Shader (for background)
// =====================================================================
// File: miss.rmiss
void main() {
    // Simple sky color (extend as needed)
    const vec3 skyColor = vec3(0.5, 0.7, 1.0);
    payloadIn = skyColor;
}