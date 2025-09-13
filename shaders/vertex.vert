#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;

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
} pushConstants;

layout(location = 0) out vec3 outBaseColor;
layout(location = 1) out float outValue;
layout(location = 2) out float outDimension;
layout(location = 3) out float outWavePhase;
layout(location = 4) out float outCycleProgress;
layout(location = 5) out float darkMatterPulse;
layout(location = 6) out float darkEnergyGlow;

void main() {
    gl_Position = pushConstants.proj * pushConstants.view * pushConstants.model * vec4(inPosition, 1.0);
    outBaseColor = pushConstants.baseColor;
    outValue = pushConstants.value;
    outDimension = pushConstants.dimension;
    outWavePhase = pushConstants.wavePhase;
    outCycleProgress = pushConstants.cycleProgress;
    darkMatterPulse = pushConstants.darkMatter;
    darkEnergyGlow = pushConstants.darkEnergy;
}