#version 450

layout(location = 0) in vec3 inPosition;

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
} push;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out float fragValue;
layout(location = 2) out float fragDarkMatter;
layout(location = 3) out float fragDarkEnergy;
layout(location = 4) out float fragWavePhase;

void main() {
    // Quad is in 2D, so ignore z-coordinate in transformations
    vec4 pos = vec4(inPosition.xy, 0.0, 1.0);
    gl_Position = push.proj * push.view * push.model * pos;
    fragColor = push.baseColor;
    fragValue = push.value;
    fragDarkMatter = push.darkMatter;
    fragDarkEnergy = push.darkEnergy;
    fragWavePhase = push.wavePhase;
}