#version 450
layout(location = 0) in float inValue;
layout(location = 1) in float inDimension;
layout(location = 2) in float inCycleProgress;
layout(location = 0) out vec4 outColor;

void main() {
    if (inDimension == 1.0) {
        outColor = vec4(1.0, 0.843, 0.0, 0.5); // Gold for 1D, semi-transparent
    } else if (inDimension == 2.0) {
        outColor = vec4(0.0, 1.0, 0.0, 1.0); // Green for 2D
    } else if (inDimension == 3.0) {
        outColor = vec4(1.0, 0.0, 0.0, 1.0); // Red for current cycle dimension (Mode 4)
    } else {
        outColor = vec4(0.0, 0.0, inValue * 0.5, 1.0); // Blue gradient for other dimensions
    }
}
