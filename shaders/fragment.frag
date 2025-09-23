#version 450
#extension GL_ARB_separate_shader_objects : enable

// Inputs from vertex shader
layout(location = 0) in vec2 TexCoord;
layout(location = 1) in vec3 BaseColor;
layout(location = 2) in float Value;
layout(location = 3) in float DimValue;
layout(location = 4) in float WavePhase;
layout(location = 5) in float CycleProgress;
layout(location = 6) in float DarkMatter;
layout(location = 7) in float DarkEnergy;

layout(location = 0) out vec4 FragColor;

// Universal equation visualization
float equation(vec2 uv, float t) {
    vec2 p = (uv - 0.5) * 10.0; // Scale and center UV
    float x = p.x;
    float y = p.y;
    float r = sqrt(x * x + y * y);
    // Incorporate push constant parameters
    return sin(r * 10.0 + t) * Value * DimValue * (1.0 + 0.5 * DarkMatter + 0.5 * DarkEnergy);
}

void main() {
    vec2 uv = TexCoord;
    float z = equation(uv, WavePhase);
    
    // Map equation output to [0, 1] for color modulation
    float intensity = (z + 1.0) * 0.5;
    
    // Combine base color with equation intensity and cycle progress
    vec3 color = BaseColor * intensity * (0.8 + 0.2 * sin(CycleProgress * 6.28));
    
    // Apply dark matter/energy influence
    color = mix(color, vec3(0.2, 0.2, 0.4), 0.3 * DarkMatter);
    color = mix(color, vec3(0.4, 0.2, 0.4), 0.3 * DarkEnergy);
    
    FragColor = vec4(color, 1.0);
}