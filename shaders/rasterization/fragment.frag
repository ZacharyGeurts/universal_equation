#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 model;       // 64 bytes
    mat4 view_proj;   // 64 bytes
    vec4 extra[8];    // 128 bytes
} push;

layout(binding = 0) uniform samplerCube envMap; // Cubemap for environment lighting

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec4 inFragColor;
layout(location = 0) out vec4 outColor;

const float PI = 3.14159265359;

// Simple diffuse IBL (approximated irradiance)
vec3 diffuseIBL(vec3 normal) {
    // Sample cubemap in normal direction for diffuse lighting
    return texture(envMap, normal).rgb;
}

// Simple specular IBL (single sample approximation)
vec3 specularIBL(vec3 normal, vec3 viewDir, float roughness) {
    // Compute reflection vector
    vec3 reflectDir = reflect(-viewDir, normal);
    // Sample cubemap with roughness-based LOD (simplified)
    float lod = roughness * 8.0; // Assume 8 mip levels
    return textureLod(envMap, reflectDir, lod).rgb;
}

void main() {
    // Normalize inputs
    vec3 N = normalize(inNormal);
    vec3 V = normalize(-inWorldPos); // View direction (assuming camera at origin in view space)

    // Material properties
    float observable = push.extra[0].x;
    vec3 baseColor = inFragColor.rgb * observable; // Modulate with observable
    float roughness = 0.5; // Fixed roughness for simplicity
    float metallic = 0.0;  // Non-metallic for diffuse focus

    // Diffuse GI
    vec3 diffuse = diffuseIBL(N) * baseColor * (1.0 - metallic);

    // Specular GI (simplified)
    vec3 specular = specularIBL(N, V, roughness) * (metallic + 0.1); // Small specular for non-metals

    // Combine lighting
    vec3 color = diffuse + specular;

    // Modulate with observable for dimension-specific effect
    color *= mix(0.5, 1.5, observable);

    // Output with alpha
    outColor = vec4(color, 1.0);
}