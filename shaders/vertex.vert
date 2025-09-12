#version 450
layout(location = 0) in vec3 inPosition; // From vertex buffer (sphere or quad vertices)
layout(location = 0) out float outValue;
layout(location = 1) out float outDimension;
layout(location = 2) out float outCycleProgress;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 proj;
    float value;
    float dimension;
    float wavePhase;
    float cycleProgress; // Only used in Mode 4
} push;

void main() {
    gl_Position = push.proj * push.view * push.model * vec4(inPosition, 1.0);
    outValue = push.value;
    outDimension = push.dimension;
    outCycleProgress = push.cycleProgress;
}
