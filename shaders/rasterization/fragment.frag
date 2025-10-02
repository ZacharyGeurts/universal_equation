#version 450

layout(location = 0) in vec3 inWorldPos; // World-space position from vertex shader

layout(push_constant) uniform PushConstants {
    mat4 model;      // Object-to-world transform
    mat4 view_proj;  // View * projection transform
    vec4 energy;     // observable, potential, darkMatter, darkEnergy
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    // Base color from energy values
    vec3 baseColor = vec3(
        clamp(0.5 + 0.5 * pc.energy.x, 0.0, 1.0), // Red from observable
        clamp(0.5 + 0.5 * pc.energy.y, 0.0, 1.0), // Green from potential
        clamp(0.5 + 0.5 * pc.energy.z, 0.0, 1.0)  // Blue from darkMatter
    );

    // Mode-specific modulation based on dimension (energy.w scales with darkEnergy)
    float dimensionFactor = clamp(pc.energy.w, 0.0, 2.0); // Normalize darkEnergy
    vec3 transformedPos = (pc.model * vec4(inWorldPos, 1.0)).xyz;
    vec3 dynamicColor = 0.5 + 0.5 * cos(transformedPos * (1.0 + dimensionFactor) + vec3(0.0, 2.0, 4.0));
    
    // Combine base and dynamic colors
    vec3 finalColor = mix(baseColor, dynamicColor, 0.5);
    
    // Alpha modulated by darkEnergy for subtle transparency effects
    float alpha = clamp(0.5 + 0.5 * pc.energy.w, 0.5, 1.0);
    
    outColor = vec4(finalColor, alpha);
}