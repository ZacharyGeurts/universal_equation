#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) rayPayloadInEXT vec4 payload;
hitAttributeEXT vec3 attribs;

layout(location = 0) in vec3 inBaseColor;
layout(location = 1) in float inValue;
layout(location = 2) in float inDimension;
layout(location = 3) in float inWavePhase;
layout(location = 4) in float inCycleProgress;
layout(location = 5) in float inDarkMatterPulse;
layout(location = 6) in float inDarkEnergyGlow;

layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0, std140) uniform Scene {
    vec3 lightDir;
    vec3 lightColor;
    vec3 ambientLight;
} scene;

const float PI = 3.1415926535;
const float INV_MAX_DIM = 1.0 / 9.0;
const float ALPHA = 5.0;
const float BETA = 0.2;
const float OMEGA = 2.0 * PI / (2.0 * 9.0 - 1.0);

// PBR parameters for gold
const float METALLIC = 0.9;
const float ROUGHNESS = 0.1; // Lower for shinier surfaces
const vec3 F0 = vec3(0.9, 0.7, 0.3); // Gold reflectivity

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
float waveVisualizer(vec3 pos, float phase) {
    float wave = sin(pos.x * 10.0 + phase * 2.0) * cos(pos.y * 8.0 + phase);
    float bars = abs(sin(pos.z * 20.0 + phase)) * (0.7 + 0.3 * inDarkMatterPulse);
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

// Fresnel-Schlick for reflection/refraction
float fresnelSchlick(float cosTheta, float ior) {
    float r0 = pow((1.0 - ior) / (1.0 + ior), 2.0);
    return r0 + (1.0 - r0) * pow(1.0 - cosTheta, 5.0);
}

// Chromatic dispersion for refraction
vec3 chromaticDispersion(vec3 dir, vec3 normal, float iorBase, float dispersion) {
    vec3 color = vec3(0.0);
    float iorR = iorBase - dispersion;
    float iorG = iorBase;
    float iorB = iorBase + dispersion;
    
    vec3 refractR = refract(dir, normal, iorR);
    vec3 refractG = refract(dir, normal, iorG);
    vec3 refractB = refract(dir, normal, iorB);
    
    color.r = refractR != vec3(0.0) ? sampleEnvironment(refractR).r : scene.ambientLight.r;
    color.g = refractG != vec3(0.0) ? sampleEnvironment(refractG).g : scene.ambientLight.g;
    color.b = refractB != vec3(0.0) ? sampleEnvironment(refractB).b : scene.ambientLight.b;
    
    return color;
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

vec3 fresnelSchlickPBR(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Reused dimensional functions
float computeDarkMatterDensity(float dim) {
    float density = inDarkMatterPulse * (1.0 + dim * INV_MAX_DIM);
    if (dim > 3.0) {
        density *= (1.0 + 0.2 * (dim - 3.0));
    }
    return max(1e-15, density);
}

float computeDarkEnergy(float distance) {
    return inDarkEnergyGlow * exp(distance * INV_MAX_DIM);
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
    float dimFrac = (dim - 1.0) / 8.0;
    vec3 worldPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 normal = normalize(attribs); // Normal from hit attributes
    vec3 viewDir = normalize(-gl_WorldRayDirectionEXT);
    vec3 lightDir = normalize(scene.lightDir);

    // Material properties
    vec3 albedo = inBaseColor; // Gold from vertex shader (0.9, 0.7, 0.3)
    float metallic = METALLIC;
    float roughness = ROUGHNESS * (1.0 - 0.2 * inDarkMatterPulse); // Dynamic roughness
    float ior = 1.5 + 0.1 * inDarkMatterPulse; // Glass-like IOR

    // PBR lighting
    vec3 V = viewDir;
    vec3 L = lightDir;
    vec3 H = normalize(V + L);
    float NdotL = max(dot(normal, L), 0.0);
    float NdotV = max(dot(normal, V), 0.0);

    vec3 F = fresnelSchlickPBR(max(dot(H, V), 0.0), F0);
    float D = distributionGGX(normal, H, roughness);
    float G = geometrySmith(normal, V, L, roughness);

    vec3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 diffuse = (1.0 - F) * (1.0 - metallic) * albedo / PI;
    vec3 directLight = (diffuse + specular) * scene.lightColor * 6.0 * NdotL; // Increased intensity

    // Wave visualizer
    float visualizer = waveVisualizer(worldPos, phase) * (1.0 + 0.2 * inDarkEnergyGlow);

    // Dimensional effects
    float collapse = computeCollapse(dim);
    float distance = abs(dim - floor(dim + 0.5));
    float darkMatter = computeDarkMatterDensity(dim) * (1.0 + 0.2 * sin(phase));
    float darkEnergy = computeDarkEnergy(distance) * (1.0 + 0.2 * cos(phase * OMEGA));

    // Reflection
    vec3 reflectDir = reflect(-viewDir, normal);
    vec4 reflectColor = vec4(sampleEnvironment(reflectDir), 1.0);
    uint rayFlags = gl_RayFlagsOpaqueEXT;
    float tMin = 0.001, tMax = 1000.0;
    traceRayEXT(topLevelAS, rayFlags, 0xFF, 0, 0, 0, worldPos, tMin, reflectDir, tMax, 0);
    reflectColor = mix(reflectColor, payload, 0.8 * metallic * (1.0 - roughness)); // Increased reflection

    // Refraction with chromatic dispersion
    float fresnelFactor = fresnelSchlick(max(dot(normal, viewDir), 0.0), ior);
    vec3 refractColor = chromaticDispersion(-viewDir, normal, ior, 0.03 * (1.0 + inDarkEnergyGlow));

    // Combine effects
    vec3 color = mix(refractColor, reflectColor.rgb, fresnelFactor);
    color += directLight; // Add PBR lighting
    color *= (0.8 + 0.2 * visualizer); // Modulate with wave
    color += 0.08 * noise(worldPos.xy * (1.0 + 0.5 * darkMatter)); // Subtle noise
    color += vec3(0.08, 0.06, 0.02) * darkEnergy * (1.0 + 0.2 * dimFrac); // Gold-tinted glow
    color += vec3(0.04, 0.04, 0.08) * darkMatter; // Subtle blue tint
    color += vec3(0.7, 0.6, 0.8) * 0.15 * inDarkEnergyGlow * dimFrac; // Subtle purple glow

    // Clamp color
    color = clamp(color, 0.0, 1.0);

    // Compute alpha
    float alpha = clamp(0.8 + 0.2 * (darkEnergy + collapse), 0.7, 1.0);

    payload = vec4(color, alpha);
}