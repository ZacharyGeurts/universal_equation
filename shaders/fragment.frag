#version 450
layout(location = 0) in vec3 fragPos;
layout(location = 0) out vec4 outColor;
layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 proj;
    float radius;
    float wavePhase;
} pc;
void main() {
    vec3 normal = normalize(fragPos);
    float glow = 0.5 + 0.5 * sin(pc.wavePhase + length(fragPos) * 2.0);
    vec3 color = vec3(0.2 + 0.3 * glow, 0.5, 0.8 - 0.3 * glow);
    outColor = vec4(color, 0.7);
}
