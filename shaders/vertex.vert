#version 460
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view_proj;
    vec4 extra[8];
} pc;

layout(location = 0) out vec3 fragNormal;
layout(location = 1) out vec2 fragTexCoord;

void main() {
    gl_Position = pc.view_proj * pc.model * vec4(inPosition, 1.0);
    fragNormal = normalize((pc.model * vec4(inNormal, 0.0)).xyz);
    // Use XY position for texture coordinates, ignoring Z due to flattening
    fragTexCoord = vec2(inPosition.x, inPosition.y) * 0.5 + 0.5; // Normalize to [0, 1]
}