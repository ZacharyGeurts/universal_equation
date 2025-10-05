#version 450

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in float inObservable;
layout(location = 3) in float inWavePhase;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view_proj;
    vec4 extra[8];
} pc;

layout(set = 0, binding = 0) uniform Mode {
    int mode;
} uMode;

// Simple noise function for modes 6+
float noise(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453123);
}

// Diffuse and specular lighting
vec3 computeLighting(vec3 normal, vec3 viewDir) {
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float diffuse = max(dot(normal, lightDir), 0.0);
    vec3 halfway = normalize(lightDir + viewDir);
    float specular = pow(max(dot(normal, halfway), 0.0), 32.0) * 0.5;
    return vec3(diffuse + specular);
}

void main() {
    vec3 viewDir = normalize(-inWorldPos);
    vec3 normal = normalize(inNormal);
    vec3 baseColor = vec3(0.2, 0.4, 0.8); // Base blue tone
    vec3 color = baseColor;
    float glow = 0.0;

    color *= computeLighting(normal, viewDir);

    switch (uMode.mode) {
        case 1: // Soft glow
            glow = inObservable * 0.3 * (1.0 + sin(inWavePhase));
            color += vec3(glow, glow * 0.5, glow * 0.8);
            break;
        case 2: // Swirling pattern
            float swirl = sin(inWorldPos.x * 5.0 + inWavePhase) * cos(inWorldPos.y * 5.0 + inWavePhase);
            color += vec3(swirl * pc.extra[1].x, swirl * pc.extra[1].x * 0.5, swirl);
            break;
        case 3: // Pulsating bands
            float band = sin(inWorldPos.y * 10.0 + inWavePhase * pc.extra[2].x);
            color *= (1.0 + band * 0.5);
            glow = inObservable * abs(band) * 0.2;
            color += vec3(glow, glow * 0.7, glow);
            break;
        case 4: // Wave patterns
            float wave = sin(dot(inWorldPos, vec3(3.0, 4.0, 5.0)) + inWavePhase * pc.extra[3].x);
            color += vec3(wave * pc.extra[1].x, wave * pc.extra[2].x, wave * pc.extra[3].x);
            glow = inObservable * 0.4 * abs(wave);
            color += vec3(glow * 0.8, glow, glow * 0.6);
            break;
        case 5: // Glowing grid
            vec2 grid = fract(inWorldPos.xy * 10.0 + inWavePhase * pc.extra[4].x);
            float gridIntensity = smoothstep(0.9, 1.0, max(grid.x, grid.y));
            color += vec3(gridIntensity * pc.extra[1].x, gridIntensity * pc.extra[2].x, gridIntensity * pc.extra[3].x);
            glow = inObservable * gridIntensity * 0.5;
            color += vec3(glow, glow * 0.8, glow * 0.9);
            break;
        case 6: // Fractal noise
            float n = noise(inWorldPos * 5.0 + inWavePhase * pc.extra[5].x);
            color += vec3(n * pc.extra[1].x, n * pc.extra[2].x, n * pc.extra[3].x);
            glow = inObservable * n * 0.3;
            color += vec3(glow * 0.7, glow, glow * 0.8);
            break;
        case 7: // Aurora gradients
            float aurora = sin(inWorldPos.y * 8.0 + inWavePhase * pc.extra[6].x) * cos(inWorldPos.x * 6.0 + inWavePhase);
            color += vec3(aurora * pc.extra[1].x, aurora * pc.extra[2].x, aurora * pc.extra[3].x);
            glow = inObservable * abs(aurora) * 0.4;
            color += vec3(glow, glow * 0.9, glow * 0.6);
            break;
        case 8: // High-frequency pulses
            float pulse = sin(dot(inWorldPos, vec3(10.0)) + inWavePhase * pc.extra[7].x);
            color += vec3(pulse * pc.extra[1].x, pulse * pc.extra[2].x, pulse * pc.extra[3].x);
            glow = inObservable * abs(pulse) * 0.5;
            color += vec3(glow * 0.9, glow * 0.7, glow);
            break;
        case 9: // Multi-layered effects
            float layer1 = sin(inWorldPos.x * 15.0 + inWavePhase * pc.extra[7].x);
            float layer2 = cos(inWorldPos.y * 12.0 + inWavePhase * pc.extra[6].x);
            float layer3 = noise(inWorldPos * 8.0 + inWavePhase * pc.extra[5].x);
            color += vec3(layer1 * pc.extra[1].x, layer2 * pc.extra[2].x, layer3 * pc.extra[3].x);
            glow = inObservable * (abs(layer1) + abs(layer2) + layer3) * 0.3;
            color += vec3(glow * 0.8, glow * 0.9, glow);
            break;
        default:
            color = baseColor;
            break;
    }

    outColor = vec4(color, 1.0);
}