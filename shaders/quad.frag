#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) in float fragValue;
layout(location = 2) in float fragDarkMatter;
layout(location = 3) in float fragDarkEnergy;
layout(location = 4) in float fragWavePhase;

layout(location = 0) out vec4 outColor;

void main() {
    // Base color with white gold aesthetic
    vec3 color = fragColor * fragValue;
    
    // Add subtle shimmer for quad
    float shimmer = 0.05 * sin(fragWavePhase * 1.5) + 0.95;
    color *= shimmer;
    
    // Modulate with dark matter and dark energy (less intense for quad)
    color += vec3(fragDarkMatter * 0.05, fragDarkEnergy * 0.1, 0.03);
    
    // Clamp to ensure valid color range
    color = clamp(color, 0.0, 1.0);
    
    outColor = vec4(color, 1.0);
}