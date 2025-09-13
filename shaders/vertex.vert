#version 450
layout(location = 0) in vec3 inPosition;
layout(location = 0) out float outValue;
layout(location = 1) out float outDimension;
layout(location = 2) out float outCycleProgress;
layout(location = 3) out float outDarkMatter;
layout(location = 4) out float outDarkEnergy;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 proj;
    float value;
    float dimension;
    float wavePhase;
    float cycleProgress;
    float darkMatter;
    float darkEnergy;
} push;

float noise(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453123);
}

vec3 displaceVertex(vec3 pos, float time, float dim) {
    float beat = sin(time * 6.28318 + push.wavePhase);
    float noiseVal = noise(pos + vec3(time)) * 0.2;
    float darkMatterAmp = push.darkMatter * 0.3; // Dark matter increases displacement
    float displace = (0.1 + 0.05 * beat + darkMatterAmp) * (1.0 + 0.5 * noiseVal);
    if (dim == 1.0) {
        return pos + vec3(0.0, displace * pos.y * 0.5, 0.0); // Subtle vertical ripple for 1D plane
    } else if (dim == 2.0) {
        return pos + vec3(displace * pos.x * 0.5, displace * pos.y * 0.5, 0.0); // Gentle XY warp for 2D
    } else if (dim == 3.0) {
        return pos + normalize(pos) * displace; // Radial pulse for 3D
    } else {
        return pos + vec3(noiseVal * displace, noiseVal * displace, noiseVal * displace); // Chaotic 4D wobble
    }
}

void main() {
    vec3 displacedPos = displaceVertex(inPosition, push.cycleProgress, push.dimension);
    gl_Position = push.proj * push.view * push.model * vec4(displacedPos, 1.0);
    outValue = push.value + 0.1 * sin(push.cycleProgress * 4.0);
    outDimension = push.dimension;
    outCycleProgress = push.cycleProgress;
    outDarkMatter = push.darkMatter;
    outDarkEnergy = push.darkEnergy;
    outValue += 0.05 * noise(inPosition + vec3(push.wavePhase)) * sin(push.cycleProgress * 2.0);
}
