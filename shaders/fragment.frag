#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view_proj;
    vec4 extra[8];
} pc;

layout(set = 0, binding = 1) uniform sampler2D energyTexture; // Pre-generated gradient or noise texture

layout(location = 0) out vec4 outColor;

void main() {
    // Extract energy values and animation offset
    float observable = pc.extra[0].x;
    float potential = pc.extra[0].y;
    float darkMatter = pc.extra[0].z;
    float darkEnergy = pc.extra[0].w;
    float rotation = pc.extra[1].x;
    float wavePhase = pc.extra[1].y;

    // Animate texture coordinates based on energy values and wavePhase
    vec2 animatedTexCoord = fragTexCoord;
    animatedTexCoord.x += sin(wavePhase + potential * 5.0) * 0.1 * observable; // Horizontal movement
    animatedTexCoord.y += cos(wavePhase + darkMatter * 5.0) * 0.1 * darkEnergy; // Vertical movement
    animatedTexCoord = fract(animatedTexCoord); // Wrap around [0, 1]

    // Sample texture with animated coordinates
    vec4 texColor = texture(energyTexture, animatedTexCoord);

    // Modulate color with energy values
    vec3 baseColor = texColor.rgb;
    baseColor *= vec3(observable, potential, darkMatter) * 0.5 + 0.5; // Energy-driven color tint
    baseColor += vec3(darkEnergy) * 0.2; // Add darkEnergy glow

    // Basic lighting
    float light = max(dot(fragNormal, vec3(0.0, 0.0, 1.0)), 0.0) * 0.7 + 0.3;
    outColor = vec4(baseColor * light, 1.0);
}