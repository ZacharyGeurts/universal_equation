#version 450

// Input: Color from vertex shader
layout(location = 0) in vec3 fragColor;

// Output: Final color to framebuffer
layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(fragColor, 1.0); // Output color with full opacity
}