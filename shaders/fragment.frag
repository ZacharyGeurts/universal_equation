#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in float inValue;
layout(location = 1) in float inDimension;
layout(location = 2) in float inWavePhase;
layout(location = 3) in float inCycleProgress;
layout(location = 4) in float darkMatterPulse;
layout(location = 5) in float darkEnergyGlow;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 color = vec3(0.0);
    float alpha = 1.0;

    if (inDimension == 1.0) {
        float intensity = inValue * (0.8 + 0.2 * cos(inWavePhase));
        color = vec3(0.0, 0.0, 1.0) * intensity;
        alpha = 0.7 + 0.3 * abs(sin(inCycleProgress * 3.14159));
    } else if (inDimension == 2.0) {
        float beat = 0.8 + 0.2 * cos(inCycleProgress * 3.14159) * darkMatterPulse;
        float glow = 0.3 * darkEnergyGlow * sin(inCycleProgress);
        color = vec3(0.0, 1.0, 0.2 + glow) * beat;
        alpha = 1.0;
    } else if (inDimension == 3.0) {
        float intensity = inValue * (0.8 + 0.2 * cos(inWavePhase));
        color = vec3(1.0, 0.0, 0.0) * intensity;
        alpha = 0.7 + 0.3 * abs(cos(inCycleProgress * 3.14159));
    } else if (inDimension == 4.0) {
        float intensity = inValue * (0.8 + 0.2 * sin(inWavePhase + inCycleProgress));
        color = vec3(1.0, 1.0, 0.0) * intensity;
        alpha = 0.7 + 0.3 * abs(sin(inWavePhase * 0.5));
    }

    outColor = vec4(color, alpha);
}
