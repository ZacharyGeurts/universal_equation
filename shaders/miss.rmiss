#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT vec4 payload;

layout(push_constant) uniform PushConstants {
    mat4 viewProj;
    vec3 camPos;
    float wavePhase;
    float cycleProgress;
    float zoomFactor;
    float interactionStrength;
    float darkMatter;
    float darkEnergy;
} push;

void main() {
    payload = vec4(0.1 + 0.2 * push.cycleProgress, 0.1, 0.3 * push.darkEnergy, 1.0);
}