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

// Funky noise function for organic displacement
float noise(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453123);
}

// Bass-driven displacement function
vec3 displaceVertex(vec3 pos, float time, float dim) {
    float beat = sin(time * 6.28318 + push.wavePhase); // Bass pulse
    float noiseVal = noise(pos + vec3(time)) * 0.2; // Organic noise
    float displace = (0.1 + 0.05 * beat) * (1.0 + 0.5 * noiseVal); // Dynamic amplitude
    if (dim == 1.0) {
        return pos + vec3(0.0, displace * pos.y, 0.0); // 1D: Vertical ripple
    } else if (dim == 2.0) {
        return pos + vec3(displace * pos.x, displace * pos.y, 0.0); // 2D: XY warp
    } else if (dim == 3.0) {
        return pos + normalize(pos) * displace; // 3D: Radial pulse
    } else {
        return pos + vec3(noiseVal * displace, noiseVal * displace, noiseVal * displace); // Other: Chaotic wobble
    }
}

void main() {
    // Apply dynamic vertex displacement for that *ba boom bass* vibe
    vec3 displacedPos = displaceVertex(inPosition, push.cycleProgress, push.dimension);
    
    // Transform position with MVP matrices
    gl_Position = push.proj * push.view * push.model * vec4(displacedPos, 1.0);
    
    // Pass through values with a touch of flair
    outValue = push.value + 0.1 * sin(push.cycleProgress * 4.0); // Add rhythmic variation
    outDimension = push.dimension;
    outCycleProgress = push.cycleProgress;
    
    // Add a subtle wobble to outValue based on position and wavePhase
    outValue += 0.05 * noise(inPosition + vec3(push.wavePhase)) * sin(push.cycleProgress * 2.0);
}
