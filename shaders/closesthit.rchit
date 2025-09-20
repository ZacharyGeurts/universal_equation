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

const float PI = 3.1415926535;
const float INV_MAX_DIM = 1.0 / 9.0;
const float ALPHA = 5.0;
const float BETA = 0.2;
const float OMEGA = 2.0 * PI / (2.0 * 9.0 - 1.0);

// Ray tracing bindings (adjust as per your setup)
layout(binding = 0, set = 0) uniform accelerationStructureEXT topLevelAS;
layout(binding = 1, set = 0, std140) uniform Scene {
    vec3 lightDir;
    vec3 lightColor;
    vec3 ambientLight;
} scene;

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

// Fresnel effect for reflection/refraction blending
float fresnel(vec3 I, vec3 N, float ior) {
    float cosi = clamp(dot(I, N), -1.0, 1.0);
    float etai = 1.0, etat = ior;
    if (cosi > 0.0) { float tmp = etai; etai = etat; etat = tmp; }
    float sint = etai / etat * sqrt(max(0.0, 1.0 - cosi * cosi));
    if (sint >= 1.0) return 1.0;
    float cost = sqrt(max(0.0, 1.0 - sint * sint));
    cosi = abs(cosi);
    float Rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost));
    float Rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost));
    return (Rs * Rs + Rp * Rp) * 0.5;
}

// Wave visualizer for dynamic motion
float waveVisualizer(vec3 pos, float phase) {
    float wave = sin(pos.x * 8.0 + phase * 1.5) * cos(pos.y * 6.0 + phase);
    float bars = abs(sin(pos.z * 15.0 + phase)) * (0.6 + 0.4 * inDarkMatterPulse);
    return mix(wave, bars, 0.5 + 0.3 * sin(phase));
}

// Chromatic dispersion for rainbow-like edges
vec3 chromaticDispersion(vec3 dir, vec3 normal, float iorBase, float dispersion) {
    vec3 color = vec3(0.0);
    float iorR = iorBase - dispersion;
    float iorG = iorBase;
    float iorB = iorBase + dispersion;
    
    // Refract for each color channel
    vec3 refractR = refract(dir, normal, iorR);
    vec3 refractG = refract(dir, normal, iorG);
    vec3 refractB = refract(dir, normal, iorB);
    
    // Trace refracted rays (simplified background color for this example)
    color.r = refractR != vec3(0.0) ? scene.ambientLight.r : 0.0;
    color.g = refractG != vec3(0.0) ? scene.ambientLight.g : 0.0;
    color.b = refractB != vec3(0.0) ? scene.ambientLight.b : 0.0;
    
    return color;
}

// Reused functions
float computeDarkMatterDensity(float dim) {
    float density = inDarkMatterPulse * (1.0 + dim * INV_MAX_DIM);
    if (dim > 3.0) {
        density *= (1.0 + 0.15 * (dim - 3.0));
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
    vec3 normal = normalize(attribs); // Assuming attribs contains the normal
    vec3 viewDir = normalize(-gl_WorldRayDirectionEXT);

    // Base glass color with tint from baseColor
    vec3 baseColor = inBaseColor * (0.8 + 0.2 * dimFrac);
    vec3 color = baseColor;

    // Add noise for subtle texture
    float n = 0.05 * noise(worldPos.xy * (1.0 + 0.5 * inDarkMatterPulse + 0.3 * sin(phase)));
    color += n;

    // Wave visualizer for dynamic distortion
    float visualizer = waveVisualizer(worldPos, phase) * (1.0 + 0.15 * inDarkEnergyGlow);

    // Dimensional effects
    float collapse = computeCollapse(dim);
    float distance = abs(dim - floor(dim + 0.5));
    float darkMatter = computeDarkMatterDensity(dim) * (1.0 + 0.1 * sin(phase));
    float darkEnergy = computeDarkEnergy(distance) * (1.0 + 0.1 * cos(phase * OMEGA));

    // Fresnel effect for reflection/refraction balance
    float ior = 1.5 + 0.1 * inDarkMatterPulse; // Glass-like IOR, modulated by dark matter
    float fresnelFactor = fresnel(viewDir, normal, ior);

    // Reflection
    vec3 reflectDir = reflect(gl_WorldRayDirectionEXT, normal);
    vec4 reflectColor = vec4(scene.ambientLight, 1.0); // Simplified; trace ray for real reflections
    // For real reflections, you'd use:
    // traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, worldPos, 0.001, reflectDir, 1000.0, 0);

    // Refraction with chromatic dispersion
    vec3 refractColor = chromaticDispersion(gl_WorldRayDirectionEXT, normal, ior, 0.02 * (1.0 + inDarkEnergyGlow));

    // Combine reflection and refraction
    color = mix(refractColor, reflectColor.rgb, fresnelFactor);
    color *= (0.7 + 0.3 * visualizer); // Modulate with visualizer
    color += baseColor * (0.2 + 0.1 * darkEnergy); // Add tinted glow
    color += vec3(0.05, 0.05, 0.1) * darkMatter; // Subtle blue from dark matter

    // Specular highlight
    float shininess = 20.0 + 15.0 * darkMatter;
    vec3 halfDir = normalize(scene.lightDir + viewDir);
    float spec = pow(max(dot(normal, halfDir), 0.0), shininess);
    color += scene.lightColor * spec * (0.8 + 0.2 * inDarkEnergyGlow);

    // Clamp color
    color = clamp(color, 0.0, 1.0);

    // Compute alpha for slight transparency
    float alpha = clamp(0.8 + 0.2 * (darkEnergy + collapse), 0.7, 1.0);

    // Output to payload
    payload = vec4(color, alpha);
}