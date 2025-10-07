#version 450

// Input: Vertex position
layout(location = 0) in vec3 inPosition;

// Output: Color to fragment shader
layout(location = 0) out vec3 fragColor;

// Uniform: MVP matrix
layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

void main() {
    // Transform vertex position to clip space
    gl_Position = ubo.proj * ubo.view * ubo.model * vec4(inPosition, 1.0);
    
    // Assign a simple color based on vertex index (for variety)
    fragColor = vec3(1.0, float(gl_VertexIndex) / 2.0, 0.0); // Reddish gradient
}