// shaders/rasterization/vertex.vert
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 0) out vec4 fragColor;
layout(binding = 0) uniform UBO {
    mat4 view;
    mat4 proj;
} ubo;
void main() {
    gl_Position = ubo.proj * ubo.view * vec4(inPosition, 1.0);
    fragColor = vec4(1.0, 0.0, 0.0, 1.0); // Red for testing
}