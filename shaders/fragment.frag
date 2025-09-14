#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inBaseColor;
layout(location = 1) in float inValue;
layout(location = 2) in float inDimension;
layout(location = 3) in float inWavePhase;
layout(location = 4) in float inCycleProgress;
layout(location = 5) in float darkMatterPulse;
layout(location = 6) in float darkEnergyGlow;

layout(location = 0) out vec4 outColor;

// Simple noise function for organic variations and pseudo-environment
float noise(vec2 seed) {
    return fract(sin(dot(seed, vec2(127.1, 311.7)) + inCycleProgress * 43758.5453) * 43758.5453);
}

// Simulates gravitational lensing effect based on dark matter density
float lensingEffect(float darkMatter, vec2 uv) {
    float distortion = darkMatter * 0.15; // Increased for metallic distortion
    return 1.0 + distortion * sin(uv.x * uv.y * 12.0 + inWavePhase);
}

// Simulates cosmic dark energy emission with metallic tint
vec3 darkEnergyEmission(float darkEnergy, float phase) {
    float glow = 0.4 + 0.6 * darkEnergy * abs(sin(phase));
    return vec3(0.25 * glow, 0.35 * glow, 0.6 * glow); // Blue-silver metallic glow
}

// Simulates metallic specular reflection with anisotropic highlights
vec3 specularReflection(float shininess, float phase, vec2 uv, float anisotropy) {
    float spec = pow(abs(sin(phase + uv.x * 8.0 + uv.y * anisotropy)), shininess);
    return vec3(spec) * (0.6 + 0.4 * darkEnergyGlow); // Bright, dark energy-driven highlight
}

// Simulates Fresnel effect for metallic surfaces
float fresnelEffect(vec2 uv, float phase) {
    float angle = abs(sin(uv.x + uv.y + phase)); // Pseudo-angle approximation
    return 0.2 + 0.8 * pow(1.0 - angle, 3.0); // Fresnel term for grazing angles
}

void main() {
    vec3 color = inBaseColor; // Start with baseColor (pink for mode 5, white for others)
    float alpha = 1.0;

    // Common parameters
    float time = inCycleProgress * 3.14159;
    vec2 uv = gl_FragCoord.xy * 0.005; // Screen-space UV for reflections
    float noiseFactor = 0.04 * noise(uv); // Increased noise for metallic texture
    float lensing = lensingEffect(darkMatterPulse, uv); // Gravitational lensing
    vec3 darkEnergyEmit = darkEnergyEmission(darkEnergyGlow, time + inWavePhase);

    // Metallic parameters
    float shininess = 15.0 + 10.0 * darkMatterPulse; // High shininess for metallic look
    float anisotropy = 0.5 + 0.5 * sin(time + inWavePhase); // Dynamic anisotropic streaks
    vec3 specular = specularReflection(shininess, inWavePhase + time, uv, anisotropy);
    float fresnel = fresnelEffect(uv, time); // Fresnel for metallic edge highlights
    vec3 metallicTint = vec3(0.8 + 0.2 * darkMatterPulse, 0.7, 0.6 + 0.4 * darkEnergyGlow); // Silver-copper tint

    if (inDimension == 1.0) {
        // Mode 1: Subtle metallic ripples
        float intensity = inValue * (0.75 + 0.25 * cos(inWavePhase + noiseFactor));
        intensity *= lensing;
        color *= intensity;
        color += darkEnergyEmit * 0.1;
        color += specular * fresnel * 0.3; // Metallic sheen
        color *= metallicTint * 0.8; // Subtle metallic coloring
        alpha = 0.7 + 0.3 * abs(sin(time * 0.5));
    } else if (inDimension == 2.0) {
        // Mode 2: Nebula-like polished metal
        float interaction = 0.8 + 0.2 * cos(time * 0.8 + noiseFactor) * darkMatterPulse;
        color *= interaction * lensing;
        color += vec3(0.05, 0.1, 0.15) * darkEnergyGlow * (0.5 + 0.5 * sin(time));
        color += darkEnergyEmit * 0.2;
        color += specular * fresnel * 0.4; // Strong metallic reflection
        color *= metallicTint * 0.9; // Pronounced metallic tint
        alpha = 0.85 + 0.15 * abs(cos(time * 0.7));
    } else if (inDimension == 3.0) {
        // Mode 3: Star-field with brushed metal highlights
        float intensity = inValue * (0.75 + 0.25 * cos(inWavePhase + noiseFactor));
        intensity *= lensing;
        color *= intensity;
        color += vec3(0.1, 0.1, 0.2) * (0.5 + 0.5 * sin(time + noiseFactor));
        color += darkEnergyEmit * 0.15;
        color += specular * fresnel * 0.35; // Glossy metallic highlights
        color *= metallicTint * 0.85; // Starry metallic sheen
        alpha = 0.7 + 0.3 * abs(cos(time * 0.6));
    } else if (inDimension == 4.0) {
        // Mode 4: Higher-dimensional metallic interference
        float intensity = inValue * (0.75 + 0.25 * sin(inWavePhase + time + noiseFactor));
        intensity *= lensing;
        color *= intensity;
        color += vec3(0.05, 0.1, 0.15) * (0.5 + 0.5 * cos(time * 1.2));
        color += darkEnergyEmit * 0.2;
        color += specular * fresnel * 0.45; // Sharp, anisotropic reflections
        color *= metallicTint * 0.9; // Strong metallic coloring
        alpha = 0.65 + 0.35 * abs(sin(inWavePhase * 0.5));
    } else {
        // Mode 5 (Heaven): Radiant pink with polished metallic collapse
        float intensity = inValue * (0.8 + 0.2 * cos(inWavePhase + noiseFactor));
        intensity *= lensing;
        color *= intensity;
        color += vec3(0.15 * darkMatterPulse, 0.0, 0.1 * darkEnergyGlow);
        color += darkEnergyEmit * 0.25;
        color += specular * fresnel * 0.5; // Bright, polished reflections
        color *= vec3(1.0, 0.7 + 0.3 * darkEnergyGlow, 0.8); // Pink-metallic tint
        alpha = 0.75 + 0.25 * abs(sin(time * 0.8 + inWavePhase));
    }

    // Energy conservation and metallic realism
    color = mix(color, normalize(color) * length(color) * 0.95, 0.3); // Boost vibrancy slightly
    color = clamp(color, 0.0, 1.0); // Physically valid colors
    outColor = vec4(color, alpha);
}