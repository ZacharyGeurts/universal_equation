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

void main() {
    vec3 color = inBaseColor; // Start with baseColor (pink for mode 5, white for others)
    float alpha = 1.0;

    if (inDimension == 1.0) {
        float intensity = inValue * (0.8 + 0.2 * cos(inWavePhase));
        color *= intensity; // Modulate white base for mode 1
        alpha = 0.7 + 0.3 * abs(sin(inCycleProgress * 3.14159));
    } else if (inDimension == 2.0) {
        float beat = 0.8 + 0.2 * cos(inCycleProgress * 3.14159) * darkMatterPulse;
        float glow = 0.3 * darkEnergyGlow * sin(inCycleProgress);
        color *= beat; // Modulate white base
        color += vec3(0.0, 0.1 * glow, 0.2 * glow); // Add green-blue tint
        alpha = 1.0;
    } else if (inDimension == 3.0) {
        float intensity = inValue * (0.8 + 0.2 * cos(inWavePhase));
        color *= intensity; // Modulate white base
        alpha = 0.7 + 0.3 * abs(cos(inCycleProgress * 3.14159));
    } else if (inDimension == 4.0) {
        float intensity = inValue * (0.8 + 0.2 * sin(inWavePhase + inCycleProgress));
        color *= intensity; // Modulate white base
        alpha = 0.7 + 0.3 * abs(sin(inWavePhase * 0.5));
    } else {
        // Mode 5 (Heaven): Use pink base color with dynamic modulation
        float intensity = inValue * (0.8 + 0.2 * cos(inWavePhase));
        color *= intensity; // Modulate pink base
        color += vec3(0.1 * darkMatterPulse, 0.0, 0.1 * darkEnergyGlow); // Add dark matter/energy tint
        alpha = 0.8 + 0.2 * abs(sin(inCycleProgress * 3.14159 + inWavePhase));
    }

    color = clamp(color, 0.0, 1.0);
    outColor = vec4(color, alpha);
}