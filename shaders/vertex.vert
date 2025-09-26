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
layout(location = 2) out vec2 outUV;

void main() {
    // Apply wave-like displacement using wavePhase and cycleProgress
    vec3 offset = inNormal * sin(push.wavePhase + push.cycleProgress + length(inPosition)) * 0.1;
    
    // Scale position by zoomLevel and offset relative to camPos
    vec3 pos = inPosition * push.zoomLevel + normalize(inPosition - push.camPos) * push.observable * 0.05;
    pos += offset; // Add wave displacement
    
    // Transform position
    gl_Position = push.viewProj * vec4(pos, 1.0);
    
    // Pass vertex color and UVs
    outColor = inColor;
    outUV = inUV;
}