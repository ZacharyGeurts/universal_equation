#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require

// Push constants
layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 baseColor;
    float value;
    float dimension;
    float wavePhase;
    float cycleProgress;
    float darkMatter;
    float darkEnergy;
    vec3 lightColor;
} push;

// Ray payload
layout(location = 0) rayPayloadInNV vec3 rayPayload;

void main() {
    // Background color (sky-like gradient)
    vec3 background = vec3(0.1, 0.2, 0.3);
    
    // Modulate with darkMatter and darkEnergy
    background *= (1.0 - push.darkMatter * 0.5) * (1.0 + push.darkEnergy * 0.3);
    
    // Apply wave modulation
    float modulation = sin(push.wavePhase + push.cycleProgress * 6.28318530718) * push.value * 0.1 + 1.0;
    background *= modulation;
    
    // Apply light color influence
    background *= push.lightColor;
    
    rayPayload = background;
}