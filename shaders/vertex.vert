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

const float INV_MAX_DIM = 1.0 / 9.0;
const float ALPHA = 5.0;
const float BETA = 0.2;
const float OMEGA = 2.0 * 3.14159265359 / (2.0 * 9.0 - 1.0);

// Wave-like displacement for visualizer effect
float waveDisplacement(float dim, float phase, float cycle) {
    return 0.05 * sin(dim * 2.0 + phase * 3.0 + cycle * OMEGA) * (0.5 + 0.5 * pushConstants.value);
}

float computeDarkMatterDensity(float dim) {
    float density = pushConstants.darkMatter * (1.0 + dim * INV_MAX_DIM);
    if (dim > 3.0) {
        density *= (1.0 + 0.15 * (dim - 3.0));
    }
    return max(1e-15, density);
}

float computeDarkEnergy(float distance) {
    return pushConstants.darkEnergy * exp(distance * INV_MAX_DIM);
}

float computeCollapse(float dim) {
    if (dim < 1.5) return 0.0;
    float phase = dim / (2.0 * 9.0);
    return pushConstants.value * dim * exp(-BETA * (dim - 1.0)) * abs(cos(2.0 * 3.14159265359 * phase));
}

void main() {
    vec3 pos = inPosition;
    
    float dim = max(1.0, pushConstants.dimension);
    float collapse = computeCollapse(dim);
    float waveDisp = waveDisplacement(dim, pushConstants.wavePhase, pushConstants.cycleProgress);
    pos += vec3(
        waveDisp * cos(pushConstants.cycleProgress * OMEGA),
        waveDisp * sin(pushConstants.wavePhase + pushConstants.cycleProgress * OMEGA),
        waveDisp * 0.5
    );

    gl_Position = pushConstants.proj * pushConstants.view * pushConstants.model * vec4(pos, 1.0);

    outBaseColor = pushConstants.baseColor;
    outValue = pushConstants.value;
    outDimension = pushConstants.dimension;
    outWavePhase = pushConstants.wavePhase;
    outCycleProgress = pushConstants.cycleProgress;

    float distance = abs(dim - floor(dim + 0.5));
    darkMatterPulse = computeDarkMatterDensity(dim) * (1.0 + 0.15 * sin(pushConstants.wavePhase + pushConstants.cycleProgress));
    darkEnergyGlow = computeDarkEnergy(distance) * (1.0 + 0.15 * cos(pushConstants.cycleProgress * OMEGA));
}