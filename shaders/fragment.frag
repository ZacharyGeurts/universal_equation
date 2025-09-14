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

// Simple noise function for organic, realistic variations
float noise(vec2 seed) {
    return fract(sin(dot(seed, vec2(127.1, 311.7)) + inCycleProgress * 43758.5453) * 43758.5453);
}

// Simulates gravitational lensing effect based on dark matter density
float lensingEffect(float darkMatter, vec2 uv) {
    float distortion = darkMatter * 0.1; // Scale distortion by dark matter strength
    return 1.0 + distortion * sin(uv.x * uv.y * 10.0 + inWavePhase);
}

// Simulates cosmic glow for dark energy
vec3 darkEnergyEmission(float darkEnergy, float phase) {
    float glow = 0.3 + 0.7 * darkEnergy * abs(sin(phase));
    return vec3(0.2 * glow, 0.3 * glow, 0.5 * glow); // Soft blue-white emission
}

void main() {
    vec3 color = inBaseColor; // Start with baseColor (pink for mode 5, white for others)
    float alpha = 1.0;

    // Common parameters inspired by UniversalEquation
    float time = inCycleProgress * 3.14159;
    float noiseFactor = 0.03 * noise(gl_FragCoord.xy * 0.01); // Subtle noise for realism
    float lensing = lensingEffect(darkMatterPulse, gl_FragCoord.xy * 0.005); // Gravitational lensing
    vec3 darkEnergyEmit = darkEnergyEmission(darkEnergyGlow, time + inWavePhase);

    if (inDimension == 1.0) {
        // Mode 1: 1D permeation, subtle energy ripples
        float intensity = inValue * (0.8 + 0.2 * cos(inWavePhase + noiseFactor));
        intensity *= lensing; // Apply lensing effect
        color *= intensity; // Modulate with observable energy
        color += darkEnergyEmit * 0.1; // Subtle dark energy glow
        alpha = 0.7 + 0.3 * abs(sin(time * 0.5)); // Smooth alpha oscillation
    } else if (inDimension == 2.0) {
        // Mode 2: 2D interactions, nebula-like glow
        float interaction = 0.8 + 0.2 * cos(time * 0.8 + noiseFactor) * darkMatterPulse;
        color *= interaction * lensing; // Modulate with dark matter interactions
        color += vec3(0.05, 0.1, 0.15) * darkEnergyGlow * (0.5 + 0.5 * sin(time)); // Nebular tint
        color += darkEnergyEmit * 0.2; // Enhanced dark energy emission
        alpha = 0.85 + 0.15 * abs(cos(time * 0.7));
    } else if (inDimension == 3.0) {
        // Mode 3: 3D influence, star-field-like shimmer
        float intensity = inValue * (0.75 + 0.25 * cos(inWavePhase + noiseFactor));
        intensity *= lensing; // Apply lensing for realism
        color *= intensity;
        color += vec3(0.1, 0.1, 0.2) * (0.5 + 0.5 * sin(time + noiseFactor)); // Starry shimmer
        color += darkEnergyEmit * 0.15; // Subtle cosmic glow
        alpha = 0.7 + 0.3 * abs(cos(time * 0.6));
    } else if (inDimension == 4.0) {
        // Mode 4: Higher-dimensional interference patterns
        float intensity = inValue * (0.75 + 0.25 * sin(inWavePhase + time + noiseFactor));
        intensity *= lensing; // Lensing for dimensional distortion
        color *= intensity;
        color += vec3(0.05, 0.1, 0.15) * (0.5 + 0.5 * cos(time * 1.2)); // Interference glow
        color += darkEnergyEmit * 0.2; // Stronger dark energy effect
        alpha = 0.65 + 0.35 * abs(sin(inWavePhase * 0.5));
    } else {
        // Mode 5 (Heaven): Cosmic pink with collapse and dark matter effects
        float intensity = inValue * (0.8 + 0.2 * cos(inWavePhase + noiseFactor));
        intensity *= lensing; // Lensing for gravitational collapse
        color *= intensity; // Modulate pink base
        color += vec3(0.15 * darkMatterPulse, 0.0, 0.1 * darkEnergyGlow); // Dark matter/energy tint
        color += darkEnergyEmit * 0.25; // Radiant cosmic glow
        alpha = 0.75 + 0.25 * abs(sin(time * 0.8 + inWavePhase));
    }

    // Energy conservation and realism
    color = mix(color, normalize(color) * length(color) * 0.9, 0.2); // Subtle vibrancy
    color = clamp(color, 0.0, 1.0); // Ensure physically valid colors
    outColor = vec4(color, alpha);
}