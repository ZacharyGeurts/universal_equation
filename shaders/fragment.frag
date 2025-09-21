#version 450
#extension GL_KHR_vulkan_glsl : enable

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragBaseColor;
layout(location = 3) in float fragValue;
layout(location = 4) in float fragWavePhase;
layout(location = 5) in float fragCycleProgress;
layout(location = 6) in float fragDarkMatter;
layout(location = 7) in float fragDarkEnergy;

layout(location = 0) out vec4 outColor;

// Constants for polished gold material (PBR)
const vec3 goldAlbedo = vec3(1.0, 0.86, 0.57); // Gold base color
const float metallic = 0.9; // High metallicity for gold
const float roughness = 0.1; // Low roughness for polished look
const vec3 lightColor = vec3(1.0, 1.0, 1.0); // White light for all dynamic lights

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 baseColor;
    float value;
    float dimValue;
    float wavePhase;
    float cycleProgress;
    float darkMatter;
    float darkEnergy;
    vec3 lightPositions[3]; // Dynamic light positions
    float lightIntensities[3]; // Dynamic light intensities
} push;

// Simple noise function for swirling effect
float noise(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
    vec3 N = normalize(fragNormal);
    vec3 V = normalize(-fragPos); // Assuming camera at origin in view space

    // Swirling pattern based on wavePhase and UniversalEquation variables
    vec2 uv = vec2(fragNormal.x + fragWavePhase, fragNormal.y + fragCycleProgress);
    float swirl = noise(uv * (1.0 + fragDarkMatter * 0.5)) * (0.7 + 0.3 * sin(fragWavePhase + fragDarkEnergy));
    swirl = clamp(swirl * fragValue, 0.0, 1.0);

    // PBR lighting calculations for multiple lights
    vec3 diffuse = vec3(0.0);
    vec3 specular = vec3(0.0);
    float alpha = roughness * roughness;

    for (int i = 0; i < 3; ++i) {
        vec3 L = normalize(push.lightPositions[i] - fragPos);
        vec3 H = normalize(L + V);
        float NdotL = max(dot(N, L), 0.0);
        float NdotH = max(dot(N, H), 0.0);
        float NdotV = max(dot(N, V), 0.0);

        // Diffuse component
        diffuse += goldAlbedo * (1.0 - metallic) * NdotL * push.lightIntensities[i];

        // Specular component (simplified Cook-Torrance)
        float spec = pow(NdotH, 1.0 / (alpha * alpha));
        specular += mix(vec3(0.04), goldAlbedo, metallic) * spec * NdotL * push.lightIntensities[i];
    }

    // Combine with swirling effect
    vec3 color = (diffuse + specular) * lightColor;
    color += swirl * goldAlbedo * 0.3; // Add swirling highlight
    color *= fragBaseColor; // Modulate with baseColor from push constants

    // Apply UniversalEquation variables for dynamic effects
    color *= (0.8 + 0.2 * fragValue); // Scale brightness with observable
    color += vec3(0.1, 0.05, 0.0) * fragDarkEnergy; // Subtle orange glow from dark energy

    outColor = vec4(color, 1.0);
}