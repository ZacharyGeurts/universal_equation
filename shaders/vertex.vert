#version 450
#extension GL_ARB_separate_shader_objects : enable

// Input attributes
layout(location = 0) in vec3 aPos; // Vertex position

// Output to fragment shader
layout(location = 0) out vec3 BaseColor;
layout(location = 1) out float Value;
layout(location = 2) out float DimValue;
layout(location = 3) out float WavePhase;
layout(location = 4) out float CycleProgress;
layout(location = 5) out float DarkMatter;
layout(location = 6) out float DarkEnergy;

// Push constants matching types_ue.hpp
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

void main() {
    gl_Position = pc.proj * pc.view * pc.model * vec4(aPos, 1.0);
    BaseColor = pc.baseColor;
    Value = pc.value;
    DimValue = pc.dimValue;
    WavePhase = pc.wavePhase;
    CycleProgress = pc.cycleProgress;
    DarkMatter = pc.darkMatter;
    DarkEnergy = pc.darkEnergy;
}