// Updated shader.vert: Pass dimension and scales to frag
#version 450

layout(location = 0) in vec3 inPosition;

layout(location = 0) out vec3 fragPos;  // Pass pos for ray

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view_proj;
    float avgProjScale;  // New
    float jitterAlpha;   // New
    int dimension;       // New
} push;

void main() {
    fragPos = inPosition;
    gl_Position = push.view_proj * push.model * vec4(inPosition, 1.0);
}