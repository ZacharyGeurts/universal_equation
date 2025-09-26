#version 450
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in vec3 aPos;

layout(set = 0, binding = 0) uniform Matrices {
    mat4 model;
    mat4 lightSpaceMatrix;
} matrices;

void main() {
    gl_Position = matrices.lightSpaceMatrix * matrices.model * vec4(aPos, 1.0);
}