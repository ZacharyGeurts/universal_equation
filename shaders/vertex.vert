#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 proj;
    float value;
    float dimension;
    float wavePhase;
    float cycleProgress;
    float darkMatter;
    float darkEnergy;
} pushConstants;

layout(location = 0) out float outValue;
layout(location = 1) out float outDimension;
layout(location = 2) out float outWavePhase;
layout(location = 3) out float outCycleProgress;
layout(location = 4) out float darkMatterPulse;
layout(location = 5) out float darkEnergyGlow;

void main() {
    gl_Position = pushConstants.proj * pushConstants.view * pushConstants.model * vec4(inPosition, 1.0);
    outValue = pushConstants.value;
    outDimension = pushConstants.dimension;
    outWavePhase = pushConstants.wavePhase;
    outCycleProgress = pushConstants.cycleProgress;
    darkMatterPulse = pushConstants.darkMatter;
    darkEnergyGlow = pushConstants.darkEnergy;
}
