#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(push_constant) uniform PushConstants {
    mat4 model;       // 64 bytes
    mat4 view_proj;   // 64 bytes
    vec4 extra[8];    // 128 bytes
} push;

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec4 outFragColor;

void main() {
    // Transform position to world space
    vec4 worldPos = push.model * vec4(inPosition, 1.0);
    gl_Position = push.view_proj * worldPos;

    // Compute normal (assuming sphere centered at origin)
    outNormal = normalize(inPosition); // Sphere normals are normalized positions
    outWorldPos = worldPos.xyz;

    // Pass observable value as base color
    float observable = push.extra[0].x;
    outFragColor = vec4(observable, 0.5, 0.5, 1.0);
}