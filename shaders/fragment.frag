#version 450
#extension GL_ARB_separate_shader_objects : enable

// Environment map for reflections and refractions
layout(binding = 0) uniform samplerCube envMap;

// Inputs from vertex shader
layout(location = 0) in vec2 TexCoord;
layout(location = 1) in vec3 BaseColor;
layout(location = 2) in float Value;
layout(location = 3) in float DimValue;
layout(location = 4) in float WavePhase;
layout(location = 5) in float CycleProgress;
layout(location = 6) in float DarkMatter;
layout(location = 7) in float DarkEnergy;
layout(location = 8) in vec3 Normal;
layout(location = 9) in vec3 WorldPos;

// Push constants (repeated for fragment stage)
layout(push_constant) uniform PushConstants {
    mat4 model;         // offset 0
    mat4 view;          // offset 64
    mat4 proj;          // offset 128
    vec3 baseColor;     // offset 192
    float value;        // offset 204
    float dimValue;     // offset 208
    float wavePhase;    // offset 212
    float cycleProgress; // offset 216
    float darkMatter;   // offset 220
    float darkEnergy;   // offset 224
} pc;

layout(location = 0) out vec4 FragColor;

// Fresnel approximation (Schlick)
float fresnelSchlick(float cosTheta, float ior) {
    float r0 = pow((1.0 - ior) / (1.0 + ior), 2.0);
    return r0 + (1.0 - r0) * pow(1.0 - cosTheta, 5.0);
}

// Checkerboard function
float checkerboard(vec2 uv, float scale) {
    vec2 p = uv * scale;
    return mod(floor(p.x) + floor(p.y), 2.0);
}

void main() {
    vec3 n = normalize(Normal);
    vec3 cameraPos = inverse(pc.view)[3].xyz;
    vec3 viewDir = normalize(cameraPos - WorldPos);
    vec3 incident = -viewDir;

    float ior = 1.5; // Index of refraction for glass
    vec3 refractVec = refract(incident, n, 1.0 / ior);
    vec3 reflectVec = reflect(incident, n);

    // Sample environment map
    vec3 refrColor = texture(envMap, refractVec).rgb;
    vec3 reflColor = texture(envMap, reflectVec).rgb;

    // Fresnel factor
    float cosTheta = max(0.0, dot(-incident, n));
    float fr = fresnelSchlick(cosTheta, ior);

    // Checkerboard tint (red and black)
    float check = checkerboard(TexCoord, 8.0); // Adjust scale as needed
    vec3 tint = mix(vec3(1.0, 0.0, 0.0), vec3(0.1, 0.1, 0.1), check); // Red and dark black for glass tint

    // Combine: reflection + tinted refraction
    vec3 glassColor = reflColor * fr + refrColor * (1.0 - fr) * tint;

    // Incorporate push constant influences (keeping original logic)
    glassColor *= (0.8 + 0.2 * sin(CycleProgress * 6.283185307));
    glassColor = mix(glassColor, vec3(0.2, 0.2, 0.4), 0.3 * DarkMatter);
    glassColor = mix(glassColor, vec3(0.4, 0.2, 0.4), 0.3 * DarkEnergy);

    // Optional: incorporate wavePhase for slight distortion
    vec3 distortion = vec3(sin(WavePhase + TexCoord.x * 10.0) * 0.01 * Value * DimValue);
    glassColor += distortion;

    FragColor = vec4(glassColor, 1.0);
}