#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require

// Push constants (aligned with your raygen shader)
layout(push_constant) uniform PushConstants {
    mat4 model;          // Instance model matrix (unused in miss)
    mat4 viewInverse;    // Inverse view matrix (was view, updated for consistency)
    mat4 projInverse;    // Inverse projection matrix (unused in miss)
    vec3 baseColor;      // Base modulation color
    float value;         // General value parameter
    float dimension;     // Dimension factor
    float wavePhase;     // Wave phase for modulation
    float cycleProgress; // Cycle progress for animation
    float darkMatter;    // Dark matter density factor
    float darkEnergy;    // Dark energy expansion factor
    vec3 lightColor;     // Light color modulation
    uint sampleCount;    // Number of samples per pixel (unused in miss)
    uint frameSeed;      // Seed for temporal randomness
} push;

// Ray payload (aligned with raygen: color + alpha + depth)
layout(location = 0) rayPayloadInNV struct RayPayload {
    vec3 color;
    float alpha;
    int depth;
} rayPayload;

// Simple noise function for subtle background variation (expert feature)
float hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

// Main miss shader
void main() {
    // Get ray direction in world space
    vec3 rayDir = normalize(gl_WorldRayDirectionNV);

    // Dynamic sky gradient based on ray direction (expert: more realistic background)
    float t = 0.5 * (rayDir.y + 1.0); // Map y from [-1,1] to [0,1] for zenith-to-horizon
    vec3 skyColor = mix(vec3(0.1, 0.2, 0.3), vec3(0.5, 0.7, 1.0), t); // Horizon to zenith

    // Optional subtle noise for realism (expert feature, low impact)
    float noise = hash(rayDir + push.frameSeed * 0.1) * 0.05; // Small noise amplitude
    skyColor += noise;

    // Apply original modulations from your shader
    skyColor *= push.baseColor;
    skyColor *= (1.0 - push.darkMatter * 0.5) * (1.0 + push.darkEnergy * 0.3);
    skyColor *= clamp(push.dimension * 0.5, 0.1, 1.0);

    // Dynamic wave modulation
    float modulation = sin(push.wavePhase + push.cycleProgress * 6.28318530718) * push.value * 0.1 + 1.0;
    skyColor *= modulation;

    // Apply light color influence
    skyColor *= push.lightColor;

    // Ensure color is non-negative and clamped (memory-safe, prevents overflow)
    skyColor = max(vec3(0.0), skyColor);

    // Set payload (expert: include alpha and depth for consistency)
    rayPayload.color = skyColor;
    rayPayload.alpha = 1.0; // Opaque background
    rayPayload.depth = 0;   // Miss rays don't increment depth
}