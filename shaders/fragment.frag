#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 inBaseColor;
layout(location = 1) in float inValue;
layout(location = 2) in float inDimension;
layout(location = 3) in float inWavePhase;
layout(location = 4) in float inCycleProgress;
layout(location = 5) in float darkMatterPulse;
layout(location = 6) in float darkEnergyGlow;

layout(location = 0) out vec4 outColor;

struct DimensionInteraction {
    int dimension;
    float distance;
    float darkMatterDensity;
};

// Simple noise function for organic variations and pseudo-environment
float noise(vec2 seed) {
    return fract(sin(dot(seed, vec2(127.1, 311.7)) + inCycleProgress * 43758.5453) * 43758.5453);
}

// Simulates gravitational lensing effect based on dark matter density
float lensingEffect(float darkMatter, vec2 uv) {
    float distortion = darkMatter * 0.2; // Increased for more pronounced metallic distortion
    return 1.0 + distortion * sin(uv.x * uv.y * 15.0 + inWavePhase * 1.5);
}

// Simulates cosmic dark energy emission with metallic tint
vec3 darkEnergyEmission(float darkEnergy, float phase) {
    float glow = 0.5 + 0.5 * darkEnergy * abs(sin(phase * 1.2));
    return vec3(0.2 * glow, 0.3 * glow, 0.7 * glow); // Enhanced blue-silver metallic glow
}

// Simulates metallic specular reflection with anisotropic highlights
vec3 specularReflection(float shininess, float phase, vec2 uv, float anisotropy) {
    float spec = pow(abs(sin(phase + uv.x * 10.0 + uv.y * anisotropy * 1.5)), shininess);
    return vec3(spec) * (0.7 + 0.3 * darkEnergyGlow); // Brighter, dynamic highlight
}

// Simulates Fresnel effect for metallic surfaces
float fresnelEffect(vec2 uv, float phase) {
    float angle = abs(sin(uv.x + uv.y * 1.2 + phase));
    return 0.3 + 0.7 * pow(1.0 - angle, 4.0); // Stronger Fresnel for metallic edges
}

float computeDarkMatterDensity(int dim, float invMaxDim, float darkMatterStrength) {
    float density = darkMatterStrength * (1.0 + float(dim) * invMaxDim);
    if (dim > 3) {
        density *= (1.0 + 0.1 * float(dim - 3));
    }
    return max(1e-15, density);
}

float computeInteraction(int currentDimension, int dim, float dist, float influence, float weak, float threeDInfluence) {
    float denom = max(1e-15, pow(float(currentDimension), float(dim)));
    float modifier = (currentDimension > 3 && dim > 3) ? weak : 1.0;
    if (currentDimension == 3 && (dim == 2 || dim == 4)) {
        modifier *= threeDInfluence;
    }
    return influence * (dist / denom) * modifier;
}

float computePermeation(int currentDimension, int dim, float oneDPermeation, float twoD, float threeDInfluence) {
    if (dim == 1 || currentDimension == 1) return oneDPermeation;
    if (currentDimension == 2 && dim > 2) return twoD;
    if (currentDimension == 3 && (dim == 2 || dim == 4)) return threeDInfluence;
    return 1.0;
}

float computeCollapse(int currentDimension, float collapse, float beta, float pi) {
    if (currentDimension == 1) return 0.0;
    float phase = float(currentDimension) / (2.0 * 9.0);
    return collapse * float(currentDimension) * exp(-beta * float(currentDimension - 1)) * abs(cos(2.0 * pi * phase));
}

float computeDarkEnergy(float dist, float darkEnergyStrength, float invMaxDim) {
    return darkEnergyStrength * exp(dist * invMaxDim);
}

void main() {
    int currentDimension = int(inDimension + 0.5); // Round to nearest int
    currentDimension = clamp(currentDimension, 1, 9);

    // Parameters from reference
    const int maxDimensions = 9;
    const float pi = 3.1415926535;
    const float influence = 1.0;
    const float weak = 0.5;
    const float collapse_param = 0.5;
    const float twoD = 0.5;
    const float threeDInfluence = 1.5;
    const float oneDPermeation = 2.0;
    const float darkMatterStrength = 0.27;
    const float darkEnergyStrength = 0.68;
    const float alpha = 5.0;
    const float beta = 0.2;
    float omega = 2.0 * pi / (2.0 * float(maxDimensions) - 1.0);
    float invMaxDim = 1.0 / float(maxDimensions);

    // Build interactions
    const int MAX_INTER = 12;
    DimensionInteraction inters[MAX_INTER];
    int numInter = 0;

    int start = max(1, currentDimension - 1);
    int end = min(maxDimensions, currentDimension + 1);
    for (int d = start; d <= end; ++d) {
        float distance = abs(float(currentDimension - d)) * (1.0 + darkEnergyStrength * invMaxDim);
        float dmd = computeDarkMatterDensity(d, invMaxDim, darkMatterStrength);
        inters[numInter].dimension = d;
        inters[numInter].distance = distance;
        inters[numInter].darkMatterDensity = dmd;
        numInter++;
    }

    for (int priv = 1; priv <= 2; ++priv) {
        if (priv != currentDimension && priv <= maxDimensions) {
            float distance = abs(float(currentDimension - priv)) * (1.0 + darkEnergyStrength * invMaxDim);
            float dmd = computeDarkMatterDensity(priv, invMaxDim, darkMatterStrength);
            inters[numInter].dimension = priv;
            inters[numInter].distance = distance;
            inters[numInter].darkMatterDensity = dmd;
            numInter++;
        }
    }

    if (currentDimension == 3 && maxDimensions >= 4) {
        for (int adj = 2; adj <= 4; adj += 2) {
            bool exists = false;
            for (int i = 0; i < numInter; ++i) {
                if (inters[i].dimension == adj) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                float distance = abs(float(currentDimension - adj)) * (1.0 + darkEnergyStrength * invMaxDim);
                float dmd = computeDarkMatterDensity(adj, invMaxDim, darkMatterStrength);
                inters[numInter].dimension = adj;
                inters[numInter].distance = distance;
                inters[numInter].darkMatterDensity = dmd;
                numInter++;
            }
        }
    }

    // Compute energy result
    float totalInfluence = influence;
    if (currentDimension >= 2) {
        totalInfluence += twoD * cos(omega * float(currentDimension));
    }
    if (currentDimension == 3) {
        totalInfluence += threeDInfluence;
    }

    float totalDarkMatter = 0.0;
    float totalDarkEnergy = 0.0;
    float interactionSum = 0.0;
    for (int i = 0; i < numInter; ++i) {
        int dim = inters[i].dimension;
        float dist = inters[i].distance;
        float dm = inters[i].darkMatterDensity;
        float inf_i = computeInteraction(currentDimension, dim, dist, influence, weak, threeDInfluence);
        float de = computeDarkEnergy(dist, darkEnergyStrength, invMaxDim);
        interactionSum += inf_i * exp(-alpha * dist) * computePermeation(currentDimension, dim, oneDPermeation, twoD, threeDInfluence) * dm;
        totalDarkMatter += dm * inf_i;
        totalDarkEnergy += de * inf_i;
    }
    totalInfluence += interactionSum;

    float coll = computeCollapse(currentDimension, collapse_param, beta, pi);

    float observable = totalInfluence + coll;
    float potential = max(0.0, totalInfluence - coll);
    float darkMatter = totalDarkMatter;
    float darkEnergy = totalDarkEnergy;

    // Normalize based on observed ranges
    float dm_pulse = darkMatter / 0.8 + darkMatterPulse * 0.5; // Combine with input
    float de_glow = (darkEnergy - 0.9) / 1.0 + darkEnergyGlow * 0.5; // Combine with input
    float coll_frac = (totalInfluence > 0.0) ? coll / totalInfluence : 0.0;

    // Common parameters
    vec3 color = inBaseColor;
    float time = inCycleProgress * pi * 2.0;
    vec2 uv = gl_FragCoord.xy * 0.004; // Adjusted for finer details
    float noiseFactor = 0.06 * noise(uv); // Increased noise for textured metal
    float lensing = lensingEffect(dm_pulse, uv);
    vec3 darkEnergyEmit = darkEnergyEmission(de_glow, time + inWavePhase);

    // Metallic parameters, modulated by equation results
    float shininess = 20.0 + 15.0 * dm_pulse; // Higher shininess
    float anisotropy = 0.6 + 0.4 * sin(time + inWavePhase + observable);
    vec3 specular = specularReflection(shininess, inWavePhase + time, uv, anisotropy);
    float fresnel = fresnelEffect(uv, time);
    vec3 metallicTint = vec3(0.85 + 0.15 * dm_pulse, 0.75 + 0.05 * coll_frac, 0.65 + 0.35 * de_glow); // Dynamic tint

    // General intensity calculation using equation results
    float intensity = inValue * (observable / 3.5) * (0.8 + 0.2 * cos(inWavePhase + time + noiseFactor));
    intensity *= lensing * (potential / 2.5 + 0.5);

    color *= intensity;
    color += darkEnergyEmit * 0.25;

    // Add interaction-based effects for multi-dimensional visuals
    float extraGlow = 0.0;
    float extraDistortion = 0.0;
    for (int i = 0; i < numInter; ++i) {
        float phase_i = inWavePhase + inters[i].distance * 3.0 + time * 0.5;
        extraGlow += 0.15 * inters[i].darkMatterDensity * sin(phase_i) * de_glow;
        extraDistortion += 0.05 * exp(-inters[i].distance) * cos(phase_i);
    }
    color += vec3(0.1, 0.15, 0.3) * extraGlow;
    lensing += extraDistortion;

    // Apply collapse effect as reddish tint
    color += vec3(0.4, 0.15, 0.1) * coll_frac * (0.4 + 0.3 * sin(time));

    // Metallic enhancements
    color += specular * fresnel * 0.5;
    color *= metallicTint * 0.9;

    // Dimension-based tint shift
    float dimFrac = float(currentDimension - 1) / 8.0;
    vec3 dimTint = mix(vec3(1.0, 1.0, 1.0), vec3(1.0, 0.6, 0.8), dimFrac); // White to pink-purple
    color *= dimTint;

    // Alpha based on potential, declared once
    //alpha = 0.6 + 0.4 * (potential / max(observable, 1e-15)); // Prevent division by zero
    //alpha = clamp(alpha, 0.4, 1.0);

    // Energy conservation and final clamp
    color = mix(color, normalize(color) * length(color) * 0.98, 0.4); // Slightly boosted vibrancy
    color = clamp(color, 0.0, 1.0);
    outColor = vec4(color, alpha);
}