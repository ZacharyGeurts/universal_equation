#version 460
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;
layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outNormal;
layout(location = 2) out vec2 outTexCoord;

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
    gl_Position = pc.proj * pc.view * pc.model * vec4(inPosition, 1.0);
    outColor = vec4(pc.baseColor, pc.value);
    outNormal = normalize((pc.model * vec4(inNormal, 0.0)).xyz);
    outTexCoord = inTexCoord;
}
