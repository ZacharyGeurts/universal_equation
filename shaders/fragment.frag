#version 450
layout(location = 0) in vec4 inColor;
layout(location = 0) out vec4 outColor;

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

void main() {
    outColor = vec4(inColor.rgb * push.observable, inColor.a);
}