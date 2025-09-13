#version 450
layout(location = 0) in float inValue;
layout(location = 1) in float inDimension;
layout(location = 2) in float inCycleProgress;
layout(location = 0) out vec4 outColor;

// Funky pulsing function to give that bass beat
float pulse(float time) {
    return 0.5 + 0.5 * sin(time * 6.28318); // Smooth oscillation (2π for full cycle)
}

void main() {
    vec3 color = vec3(0.0);
    float alpha = 1.0;
    
    // Bass-driven color vibes based on dimension
    if (inDimension == 1.0) {
        // Gold with a pulsing glow for 1D
        float glow = 0.7 + 0.3 * pulse(inCycleProgress);
        color = vec3(1.0, 0.843, 0.0) * glow; // Gold with dynamic intensity
        alpha = 0.6 + 0.2 * pulse(inCycleProgress); // Semi-transparent with pulse
    } else if (inDimension == 2.0) {
        // Green with a smooth gradient vibe
        float beat = 0.8 + 0.2 * cos(inCycleProgress * 3.14159); // Subtle bass thump
        color = vec3(0.0, 1.0, 0.2) * beat; // Neon green with a twist
        alpha = 1.0;
    } else if (inDimension == 3.0) {
        // Red with a fiery, rhythmic pulse for 3D
        float intensity = 0.9 + 0.1 * pulse(inCycleProgress * 2.0); // Faster pulse
        color = vec3(1.0, 0.1, 0.1) * intensity; // Vibrant red
        alpha = 1.0;
    } else {
        // Blue gradient with a wavy, bass-heavy feel
        float wave = 0.5 + 0.5 * sin(inValue * 3.14159 + inCycleProgress * 4.0);
        color = vec3(0.0, 0.2, 0.8 + 0.2 * wave); // Deep blue with dynamic highlights
        alpha = 0.8 + 0.2 * wave; // Slightly transparent with rhythm
    }

    // Add a subtle vignette effect based on inValue for extra depth
    float vignette = smoothstep(0.0, 1.0, 1.0 - abs(inValue - 0.5) * 0.5);
    color *= vignette;

    // Output the final color with that *ba boom bass* energy
    outColor = vec4(color, alpha);
}
