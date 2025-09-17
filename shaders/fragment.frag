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

// Constants
const float PI = 3.1415926535;
const float INV_MAX_DIM = 1.0 / 9.0;
const float ALPHA = 5.0;
const float BETA = 0.2;
const float OMEGA = 2.0 * PI / (2.0 * 9.0 - 1.0);

// Simple noise for subtle texture
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 uv) {
    vec2 i = floor(uv);
    vec2 f = fract(uv);
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(
        mix(hash(i + vec2(0.0, 0.0)), hash(i + vec2(1.0, 0.0)), u.x),
        mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x),
        u.y
    );
}

// Wave visualizer for dynamic motion
float waveVisualizer(vec2 uv, float phase) {
    float wave = sin(uv.x * 8.0 + phase * 1.5) * cos(uv.y * 6.0 + phase);
    float bars = abs(sin(uv.y * 15.0 + phase)) * (0.6 + 0.4 * darkMatterPulse);
    return mix(wave, bars, 0.5 + 0.3 * sin(phase));
}

// White-gold specular highlights
vec3 specularReflection(float shininess, float phase, vec2 uv) {
    float spec = pow(abs(sin(phase + uv.y * 12.0)), shininess);
    return vec3(0.95, 0.9, 0.85) * spec * (0.8 + 0.2 * darkEnergyGlow); // White-gold tint
}

// Edge enhancement for crisp details
float edgeEnhancement(vec2 uv, float phase, float dimFrac) {
    float edge = abs(sin(uv.y * 20.0 + phase));
    return mix(0.4, 1.0, edge) * (0.7 + 0.3 * dimFrac);
}

// Reused functions from vertex shader
float computeDarkMatterDensity(float dim) {
    float density = darkMatterPulse * (1.0 + dim * INV_MAX_DIM);
    if (dim > 3.0) {
        density *= (1.0 + 0.15 * (dim - 3.0));
    }
    return max(1e-15, density);
}

float computeDarkEnergy(float distance) {
    return darkEnergyGlow * exp(distance * INV_MAX_DIM);
}

float computeCollapse(float dim) {
    if (dim < 1.5) return 0.0;
    float phase = dim / (2.0 * 9.0);
    return inValue * dim * exp(-BETA * (dim - 1.0)) * abs(cos(2.0 * PI * phase));
}

void main() {
    // Initialize variables
    float dim = max(1.0, inDimension);
    float phase = inWavePhase + inCycleProgress * PI * 2.0;
    vec2 uv = gl_FragCoord.xy * 0.005; // Screen-space UV for effects
    float dimFrac = (dim - 1.0) / 8.0; // Normalized dimension [0,1]

    // Base white-gold color
    vec3 color = vec3(0.95, 0.9, 0.85); // White-gold base (slightly warm metallic)

    // Add noise for subtle texture
    float n = 0.1 * noise(uv * (1.0 + 0.5 * darkMatterPulse + 0.3 * sin(phase)));
    color += n;

    // Wave visualizer for dynamic effect
    float visualizer = waveVisualizer(uv, phase) * (1.0 + 0.15 * darkEnergyGlow);

    // Dimensional effects
    float collapse = computeCollapse(dim);
    float distance = abs(dim - floor(dim + 0.5));
    float darkMatter = computeDarkMatterDensity(dim) * (1.0 + 0.1 * sin(phase));
    float darkEnergy = computeDarkEnergy(distance) * (1.0 + 0.1 * cos(phase * OMEGA));

    // Specular highlights for metallic sheen
    float shininess = 10.0 + 15.0 * darkMatter;
    vec3 specular = specularReflection(shininess, phase, uv);

    // Edge enhancement for crisp details
    float edge = edgeEnhancement(uv, phase, dimFrac);

    // Combine effects
    color *= inValue * (0.7 + 0.3 * visualizer); // Modulate with visualizer
    color += specular * edge * 0.5; // Add specular highlights
    color += vec3(0.1, 0.05, 0.0) * darkEnergy; // Warm glow from dark energy
    color += vec3(0.05, 0.05, 0.1) * darkMatter; // Subtle blue tint from dark matter
    color *= vec3(1.0, 0.95, 0.9) * (0.9 + 0.1 * dimFrac); // Dimensional tint

    // Ensure color is clamped
    color = clamp(color, 0.0, 1.0);

    // Compute alpha for slight transparency effects
    float alpha = clamp(0.8 + 0.2 * (darkEnergy + collapse), 0.7, 1.0);

    // Output final color
    outColor = vec4(color, alpha);
}