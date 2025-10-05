#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view_proj;
    vec4 extra[8];
} pc;

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outWorldPos;
layout(location = 2) out float outObservable;
layout(location = 3) out float outWavePhase;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = pc.view_proj * worldPos;
    outNormal = normalize(mat3(pc.model) * inNormal);
    outWorldPos = worldPos.xyz;
    outObservable = pc.extra[0].x;
    outWavePhase = pc.extra[1].x;
}