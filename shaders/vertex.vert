#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

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
layout(location = 5) out float outDarkMatterPulse;
layout(location = 6) out float outDarkEnergyGlow;
layout(location = 7) out vec3 outNormal;
layout(location = 8) out vec3 outTangent;
layout(location = 9) out vec3 outWorldPos;
layout(location = 10) out vec2 outTexCoord;

const float INV_MAX_DIM = 1.0 / 9.0;
const float ALPHA = 5.0;
const float BETA = 0.2;
const float OMEGA = 2.0 * 3.14159265359 / (2.0 * 9.0 - 1.0);

// Crystal-like displacement for subtle faceting
float crystalDisplacement(float dim, float phase, float cycle) {
    float wave = 0.05 * sin(dim * 2.0 + phase * 3.0 + cycle * OMEGA) * (0.5 + 0.5 * pushConstants.value);
    float facet = 0.03 * sin(inPosition.x * 12.0 + phase) * cos(inPosition.y * 10.0 + cycle);
    return wave + facet * (1.0 + 0.2 * pushConstants.darkMatter);
}

float computeDarkMatterDensity(float dim) {
    float density = pushConstants.darkMatter * (1.0 + dim * INV_MAX_DIM);
    if (dim > 3.0) {
        density *= (1.0 + 0.2 * (dim - 3.0));
    }
    return max(1e-15, density);
}

float computeDarkEnergy(float distance) {
    return pushConstants.darkEnergy * exp(distance * INV_MAX_DIM) * (1.0 + 0.1 * sin(pushConstants.cycleProgress));
}

float computeCollapse(float dim) {
    if (dim < 1.5) return 0.0;
    float phase = dim / (2.0 * 9.0);
    return pushConstants.value * dim * exp(-BETA * (dim - 1.0)) * abs(cos(2.0 * 3.14159265359 * phase));
}

// Simple normal perturbation for dynamic surfaces
vec3 perturbNormal(vec3 normal, float dim, float phase) {
    float perturb = 0.1 * sin(dim + phase);
    return normalize(normal + vec3(perturb * sin(inPosition.x * 5.0), perturb * cos(inPosition.y * 5.0), 0.0));
}

void main() {
    vec3 pos = inPosition;
    float dim = max(1.0, pushConstants.dimension);
    float collapse = computeCollapse(dim);
    float crystalDisp = crystalDisplacement(dim, pushConstants.wavePhase, pushConstants.cycleProgress);
    
    // Apply displacement
    pos += vec3(
        crystalDisp * cos(pushConstants.cycleProgress * OMEGA),
        crystalDisp * sin(pushConstants.wavePhase + pushConstants.cycleProgress * OMEGA),
        crystalDisp * 0.5
    ) * (1.0 + 0.1 * collapse);

    // Transform to world space
    vec4 worldPos = pushConstants.model * vec4(pos, 1.0);
    outWorldPos = worldPos.xyz;
    gl_Position = pushConstants.proj * pushConstants.view * worldPos;

    // Compute normal in world space
    outNormal = normalize(mat3(pushConstants.model) * perturbNormal(inNormal, dim, pushConstants.wavePhase));
    
    // Compute tangent (simple approximation)
    vec3 tangent = normalize(cross(outNormal, vec3(0.0, 1.0, 0.0)));
    outTangent = normalize(mat3(pushConstants.model) * tangent);
    
    // Pass other attributes
    outBaseColor = vec3(0.9, 0.7, 0.3); // Gold base color
    outValue = pushConstants.value;
    outDimension = pushConstants.dimension;
    outWavePhase = pushConstants.wavePhase;
    outCycleProgress = pushConstants.cycleProgress;
    outTexCoord = inTexCoord;

    float distance = abs(dim - floor(dim + 0.5));
    outDarkMatterPulse = computeDarkMatterDensity(dim) * (1.0 + 0.2 * sin(pushConstants.wavePhase + pushConstants.cycleProgress));
    outDarkEnergyGlow = computeDarkEnergy(distance) * (1.0 + 0.2 * cos(pushConstants.cycleProgress * OMEGA));
}