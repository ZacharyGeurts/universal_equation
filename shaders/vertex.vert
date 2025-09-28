#version 450

layout(location = 0) in vec3 inPosition; // Matches sphere geometry (vec3 from AMOURANTH::initializeSphereGeometry)

layout(push_constant) uniform PushConstants {
    mat4 model;      // Object-to-world transform
    mat4 view_proj;  // View * projection transform
    vec4 energy;     // observable, potential, darkMatter, darkEnergy
} pc;

layout(location = 0) out vec3 outWorldPos; // Pass world-space position to fragment shader

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    outWorldPos = worldPos.xyz;
    gl_Position = pc.view_proj * worldPos;
}