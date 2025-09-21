#version 460

layout(location = 0) in vec3 inBaseColor;
layout(location = 1) in float inValue;
layout(location = 2) in float inDimension;
layout(location = 3) in float inWavePhase;
layout(location = 4) in float inCycleProgress;
layout(location = 5) in float inDarkMatterPulse;
layout(location = 6) in float inDarkEnergyGlow;
layout(location = 7) in vec3 inNormal;
layout(location = 8) in vec3 inTangent;
layout(location = 9) in vec3 inWorldPos;
layout(location = 10) in vec2 inTexCoord;

layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 1) uniform sampler2D envMap; // Environment map for reflections

// Constants
const float PI = 3.1415926535;
const float INV_MAX_DIM = 1.0 / 9.0;
const float ALPHA = 5.0;
const float BETA = 0.2;
const float OMEGA = 2.0 * PI / (2.0 * 9.0 - 1.0);

// PBR parameters for gold
const float METALLIC = 0.9; // High for metallic surface
const float ROUGHNESS = 0.1; // Lower for shinier surfaces
const vec3 F0 = vec3(0.9, 0.7, 0.3); // Gold reflectivity

// Light properties
const vec3 LIGHT_DIR = vec3(0.577, 0.577, 0.577); // Normalized directional light
const vec3 LIGHT_COLOR = vec3(1.0, 1.0, 1.0); // White directional light
const float LIGHT_INTENSITY = 6.0; // Slightly increased for stronger highlights

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
    float wave = sin(uv.x * 10.0 + phase * 2.0) * cos(uv.y * 8.0 + phase);
    float bars = abs(sin(uv.y * 20.0 + phase)) * (0.7 + 0.3 * inDarkMatterPulse);
    return mix(wave, bars, 0.5 + 0.4 * sin(phase));
}

// Procedural environment map (cosmic sky)
vec3 sampleEnvironment(vec3 dir) {
    float t = inCycleProgress * OMEGA;
    vec3 sky = vec3(0.03, 0.03, 0.08); // Dark cosmic background
    sky += 0.15 * vec3(0.8, 0.9, 1.0) * pow(abs(sin(dir.y * 10.0 + t)), 2.0); // Softer star highlights
    sky += 0.08 * inDarkEnergyGlow * vec3(0.9, 0.7, 0.3) * noise(dir.xy * 5.0); // Gold-tinted nebula glow
    return sky;
}

// PBR functions
float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return geometrySchlickGGX(NdotV, roughness) * geometrySchlickGGX(NdotL, roughness);
}

vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

void main() {
    // Initialize variables
    float dim = max(1.0, inDimension);
    float phase = inWavePhase + inCycleProgress * PI * 2.0;
    vec2 uv = inTexCoord; // Use texture coordinates for effects
    float dimFrac = (dim - 1.0) / 8.0;

    // Base material properties
    vec3 albedo = inBaseColor; // Gold base from vertex shader (0.9, 0.7, 0.3)
    float metallic = METALLIC;
    float roughness = ROUGHNESS * (1.0 - 0.2 * inDarkMatterPulse); // Dynamic roughness

    // Lighting setup
    vec3 N = normalize(inNormal);
    vec3 V = normalize(-inWorldPos); // Assuming camera at origin
    vec3 L = normalize(LIGHT_DIR); // Directional light
    vec3 H = normalize(V + L);
    vec3 R = reflect(-V, N);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    // PBR lighting with subtle attenuation
    float distance = length(inWorldPos); // Distance from origin
    float attenuation = 1.0 / (1.0 + 0.01 * distance); // Soft attenuation
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);
    float D = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 diffuse = (1.0 - F) * (1.0 - metallic) * albedo / PI;
    vec3 directLight = (diffuse + specular) * LIGHT_COLOR * LIGHT_INTENSITY * NdotL * attenuation;

    // Reflections using environment map
    vec3 envColor = texture(envMap, R.xy).rgb; // Sample environment map
    vec3 reflection = mix(sampleEnvironment(R), envColor, 0.5); // Increased envMap influence
    reflection = mix(reflection, envColor, metallic * (1.0 - roughness));

    // Wave visualizer
    float visualizer = waveVisualizer(uv, phase) * (1.0 + 0.2 * inDarkEnergyGlow);

    // Dimensional effects
    float collapse = inValue * dim * exp(-BETA * (dim - 1.0)) * abs(cos(2.0 * PI * dim / (2.0 * 9.0)));
    float darkMatter = inDarkMatterPulse * (1.0 + 0.2 * sin(phase));
    float darkEnergy = inDarkEnergyGlow * (1.0 + 0.2 * cos(phase * OMEGA));

    // Combine effects
    vec3 color = albedo;
    color += 0.08 * noise(uv * (1.0 + 0.5 * darkMatter)); // Subtle noise
    color *= (0.8 + 0.2 * visualizer); // Modulate with wave
    color += directLight; // Add PBR lighting
    color += reflection * F * (1.0 - roughness); // Increased reflection intensity
    color += vec3(0.08, 0.06, 0.02) * darkEnergy * (1.0 + 0.2 * dimFrac); // Gold-tinted warm glow
    color += vec3(0.04, 0.04, 0.08) * darkMatter; // Subtle blue tint

    // Atmospheric glow (tied to darkEnergy)
    float glow = 0.15 * inDarkEnergyGlow * (1.0 + 0.5 * sin(phase));
    color += vec3(0.7, 0.6, 0.8) * glow * dimFrac; // Subtle purple-hued glow

    // Clamp color
    color = clamp(color, 0.0, 1.0);

    // Compute alpha with glow
    float alpha = clamp(0.8 + 0.2 * (darkEnergy + collapse + glow), 0.7, 1.0);

    outColor = vec4(color, alpha);
}