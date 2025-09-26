#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 7) in vec4 inTangent;
layout(location = 8) in vec4 inBitangent;
layout(location = 9) in vec4 inColor;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec3 camPos;
    float wavePhase;
    float cycleProgress;
    float zoomLevel;
    float observable;
    float darkMatter;
    float darkEnergy;
    vec4 extraData;
} push;

layout(location = 0) out vec4 outColor;

void main() {
    gl_Position = push.viewProj * vec4(inPosition * push.zoomLevel, 1.0);
    outColor = inColor;
}