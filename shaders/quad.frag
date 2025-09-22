#version 460
layout(location = 0) in vec4 inColor;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 baseColor;
    float value;
    float dimValue;
    float wavePhase;
    float cycleProgress;
    float darkMatter;
    float darkEnergy;
} pc;

void main() {
    vec3 lightPos = vec3(10.0 * sin(pc.wavePhase), 5.0, 10.0 * cos(pc.wavePhase));
    vec3 world...
