#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec3 fragPos;
layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 proj;
    float radius;
    float wavePhase;
} pc;
void main() {
    gl_Position = pc.proj * pc.view * pc.model * vec4(inPosition * pc.radius, 1.0);
    fragPos = inPosition;
}
