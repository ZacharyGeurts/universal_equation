#version 450
layout(location = 0) in float inValue;
layout(location = 1) in float inDimension;
layout(location = 2) in float inCycleProgress;
layout(location = 3) in float inDarkMatter;
layout(location = 4) in float inDarkEnergy;
layout(location = 0) out vec4 outColor;

float pulse(float time) {
    return 0.5 + 0.5 * sin(time * 6.28318);
}

float noise(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

void main() {
    vec3 color = vec3(0.0);
    float alpha = 1.0;
    float darkEnergyGlow = inDarkEnergy * 0.5; // Dark energy adds glow
    float darkMatterPulse = 1.0 + inDarkMatter * 0.3; // Dark matter amplifies pulsing

    if (inDimension == 1.0) {
        // 1D: Divine gold aura with dark energy ripples
        float glow = 0.7 + 0.3 * pulse(inCycleProgress) * darkMatterPulse;
        float ripple = 0.2 * noise(gl_FragCoord.xy * 0.01 + inCycleProgress) * darkEnergyGlow;
        color = vec3(1.0, 0.843, 0.0) * (glow + ripple); // Gold with ethereal ripples
        alpha = 0.6 + 0.2 * pulse(inCycleProgress) + darkEnergyGlow;
    } else if (inDimension == 2.0) {
        // 2D: Neon green with smooth gradient
        float beat = 0.8 + 0.2 * cos(inCycleProgress * 3.14159) * darkMatterPulse;
        float glow = 0.3 * darkEnergyGlow * sin(inCycleProgress);
        color = vec3(0.0, 1.0, 0.2 + glow) * beat;
        alpha = 1.0;
    } else if (inDimension == 3.0) {
        // 3D: Fiery red with intense pulse
        float intensity = 0.9 + 0.1 * pulse(inCycleProgress * 2.0) * darkMatterPulse;
        float flare = 0.2 * darkEnergyGlow * cos(inCycleProgress * 4.0);
        color = vec3(1.0, 0.1 + flare, 0.1) * intensity;
        alpha = 1.0;
    } else {
        // 4D: Chaotic blue with dynamic wobble
        float wave = 0.5 + 0.5 * sin(inValue * 3.14159 + inCycleProgress * 4.0);
        float noiseVal = 0.3 * noise(gl_FragCoord.xy * 0.02 + inCycleProgress) * darkEnergyGlow;
        color = vec3(0.0, 0.2, 0.8 + wave + noiseVal) * darkMatterPulse;
        alpha = 0.8 + 0.2 * wave;
    }

    float vignette = smoothstep(0.0, 1.0, 1.0 - abs(inValue - 0.5) * 0.5);
    color *= vignette;
    outColor = vec4(color, alpha);
}
