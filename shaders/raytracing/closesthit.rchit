#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadEXT vec4 payload;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view_proj;
    vec4 extra[8];
} pc;

layout(set = 0, binding = 1) uniform Mode {
    int mode;
} uMode;

// Simple noise function for modes 6+
float noise(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453123);
}

void main() {
    vec3 hitPos = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
    vec3 normal = normalize(hitPos); // Simple normal for sphere at origin (no hitAttribute needed)
    vec3 viewDir = normalize(-gl_WorldRayDirectionEXT);
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));
    float observable = pc.extra[0].x;
    float wavePhase = pc.extra[1].x;
    vec3 baseColor = vec3(0.2, 0.4, 0.8); // Base blue tone
    vec3 color = baseColor;
    float glow = 0.0;

    // Diffuse and specular lighting
    float diffuse = max(dot(normal, lightDir), 0.0);
    vec3 halfway = normalize(lightDir + viewDir);
    float specular = pow(max(dot(normal, halfway), 0.0), 32.0) * 0.5;
    color *= (diffuse + specular);

    switch (uMode.mode) {
        case 1: // Soft glow
            glow = observable * 0.3 * (1.0 + sin(wavePhase));
            color += vec3(glow, glow * 0.5, glow * 0.8);
            break;
        case 2: // Swirling pattern
            float swirl = sin(hitPos.x * 5.0 + wavePhase) * cos(hitPos.y * 5.0 + wavePhase);
            color += vec3(swirl * pc.extra[1].x, swirl * pc.extra[1].x * 0.5, swirl);
            break;
        case 3: // Pulsating bands
            float band = sin(hitPos.y * 10.0 + wavePhase * pc.extra[2].x);
            color *= (1.0 + band * 0.5);
            glow = observable * abs(band) * 0.2;
            color += vec3(glow, glow * 0.7, glow);
            break;
        case 4: // Wave patterns
            float wave = sin(dot(hitPos, vec3(3.0, 4.0, 5.0)) + wavePhase * pc.extra[3].x);
            color += vec3(wave * pc.extra[1].x, wave * pc.extra[2].x, wave * pc.extra[3].x);
            glow = observable * 0.4 * abs(wave);
            color += vec3(glow * 0.8, glow, glow * 0.6);
            break;
        case 5: // Glowing grid
            vec2 grid = fract(hitPos.xy * 10.0 + wavePhase * pc.extra[4].x);
            float gridIntensity = smoothstep(0.9, 1.0, max(grid.x, grid.y));
            color += vec3(gridIntensity * pc.extra[1].x, gridIntensity * pc.extra[2].x, gridIntensity * pc.extra[3].x);
            glow = observable * gridIntensity * 0.5;
            color += vec3(glow, glow * 0.8, glow * 0.9);
            break;
        case 6: // Fractal noise
            float n = noise(hitPos * 5.0 + wavePhase * pc.extra[5].x);
            color += vec3(n * pc.extra[1].x, n * pc.extra[2].x, n * pc.extra[3].x);
            glow = observable * n * 0.3;
            color += vec3(glow * 0.7, glow, glow * 0.8);
            break;
        case 7: // Aurora gradients
            float aurora = sin(hitPos.y * 8.0 + wavePhase * pc.extra[6].x) * cos(hitPos.x * 6.0 + wavePhase);
            color += vec3(aurora * pc.extra[1].x, aurora * pc.extra[2].x, aurora * pc.extra[3].x);
            glow = observable * abs(aurora) * 0.4;
            color += vec3(glow, glow * 0.9, glow * 0.6);
            break;
        case 8: // High-frequency pulses
            float pulse = sin(dot(hitPos, vec3(10.0)) + wavePhase * pc.extra[7].x);
            color += vec3(pulse * pc.extra[1].x, pulse * pc.extra[2].x, pulse * pc.extra[3].x);
            glow = observable * abs(pulse) * 0.5;
            color += vec3(glow * 0.9, glow * 0.7, glow);
            break;
        case 9: // Multi-layered effects
            float layer1 = sin(hitPos.x * 15.0 + wavePhase * pc.extra[7].x);
            float layer2 = cos(hitPos.y * 12.0 + wavePhase * pc.extra[6].x);
            float layer3 = noise(hitPos * 8.0 + wavePhase * pc.extra[5].x);
            color += vec3(layer1 * pc.extra[1].x, layer2 * pc.extra[2].x, layer3 * pc.extra[3].x);
            glow = observable * (abs(layer1) + abs(layer2) + layer3) * 0.3;
            color += vec3(glow * 0.8, glow * 0.9, glow);
            break;
        default:
            color = baseColor;
            break;
    }

    payload = vec4(color, 1.0);
}