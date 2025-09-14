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

// Multi-octave noise for richer metal microtexture
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 uv) {
    float n = 0.0, amp = 0.5, f = 1.0;
    for (int i = 0; i < 4; ++i) {
        n += amp * hash(uv * f + inCycleProgress * 0.13 * float(i+1));
        f *= 2.1;
        amp *= 0.5;
    }
    return n;
}

// Sharper, more colorful lensing effect
float lensingEffect(float darkMatter, vec2 uv, float phase) {
    float d = darkMatter * 0.33;
    float swirl = sin(uv.x * uv.y * 22.0 + phase * 1.2) * cos(uv.x * 8.0 + uv.y * 10.0 + phase * 0.8);
    float pattern = 1.0 + d * swirl;
    return pattern + 0.04 * sin(uv.x * 18.0 + uv.y * 11.0 + phase);
}

// Deep cosmic blue/purple energy emission, more dimension-dependent
vec3 darkEnergyEmission(float darkEnergy, float phase, float dimFrac) {
    float e = 0.7 + 0.5 * darkEnergy * abs(sin(phase * 1.27));
    float blue = 0.3 + 0.8 * e;
    float violet = 0.6 * e * dimFrac;
    float teal = 0.18 * (1.0 - dimFrac) * e;
    return vec3(teal + 0.03, violet + 0.07, blue + 0.1);
}

// Sharper, more dynamic specular, with color shift
vec3 specularReflection(float shininess, float phase, vec2 uv, float anisotropy, float colorShift) {
    float spec = pow(abs(sin(phase + uv.x * 9.0 + uv.y * anisotropy * 2.2)), shininess);
    return vec3(0.7 + 0.25 * colorShift, 0.6 + 0.15 * colorShift, 0.8 - 0.1 * colorShift) * spec * (0.8 + 0.4 * darkEnergyGlow);
}

// Fresnel with chromatic edge bias
float fresnelEffect(vec2 uv, float phase, float dimFrac) {
    float angle = abs(sin(uv.x + uv.y * 1.1 + phase));
    float edge = pow(1.0 - angle, 3.7);
    return mix(0.22, 0.92, edge) + 0.13 * dimFrac;
}

float computeDarkMatterDensity(int dim, float invMaxDim, float darkMatterStrength) {
    float density = darkMatterStrength * (1.0 + float(dim) * invMaxDim);
    if (dim > 3) density *= (1.0 + 0.1 * float(dim - 3));
    return max(1e-15, density);
}
float computeInteraction(int currentDimension, int dim, float dist, float influence, float weak, float threeDInfluence) {
    float denom = max(1e-15, pow(float(currentDimension), float(dim)));
    float modifier = (currentDimension > 3 && dim > 3) ? weak : 1.0;
    if (currentDimension == 3 && (dim == 2 || dim == 4)) modifier *= threeDInfluence;
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
    int currentDimension = int(inDimension + 0.5);
    currentDimension = clamp(currentDimension, 1, 9);
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
                if (inters[i].dimension == adj) { exists = true; break; }
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

    float totalInfluence = influence;
    if (currentDimension >= 2) totalInfluence += twoD * cos(omega * float(currentDimension));
    if (currentDimension == 3) totalInfluence += threeDInfluence;

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

    // Nonlinear normalization for richer dynamic range
    float dm_pulse = pow(clamp(darkMatter / 0.8 + darkMatterPulse * 0.5, 0.0, 2.0), 0.78);
    float de_glow = pow(clamp((darkEnergy - 0.9) / 1.0 + darkEnergyGlow * 0.5, 0.0, 2.0), 0.95);
    float coll_frac = (totalInfluence > 0.0) ? clamp(coll / totalInfluence, 0.0, 1.0) : 0.0;

    vec3 color = inBaseColor;
    float time = inCycleProgress * pi * 2.0;
    vec2 uv = gl_FragCoord.xy * 0.004;
    float n = 0.10 * noise(uv * (0.8 + 0.7 * dm_pulse + 0.5 * sin(time)));
    float phase = inWavePhase + time;
    float dimFrac = float(currentDimension - 1) / 8.0;

    float lensing = lensingEffect(dm_pulse, uv, phase) * (1.0 + 0.07 * de_glow);
    vec3 darkEnergyEmit = darkEnergyEmission(de_glow, phase, dimFrac);

    // Metallic color modulated by energy
    float shininess = 17.0 + 21.0 * pow(dm_pulse, 1.12);
    float anisotropy = 0.7 + 0.45 * sin(phase + observable);
    float colorShift = 0.3 + 0.7 * de_glow;
    vec3 specular = specularReflection(shininess, phase, uv, anisotropy, colorShift);

    float fresnel = fresnelEffect(uv, phase, dimFrac);

    // Multi-channel metallic tint with nonlinear shifts
    vec3 metallicTint = vec3(
        0.86 + 0.17 * dm_pulse + 0.09 * de_glow,
        0.74 + 0.15 * coll_frac + 0.08 * dm_pulse,
        0.62 + 0.39 * de_glow - 0.09 * coll_frac
    );

    // Intensity with adaptive energy mapping
    float intensity = inValue * pow(observable / 3.4, 0.86) * (0.7 + 0.3 * cos(phase + n));
    intensity *= lensing * (pow(max(potential, 0.0) / 2.4 + 0.6, 0.86));

    color *= intensity;
    color += darkEnergyEmit * (0.22 + 0.06 * dm_pulse);

    // Inter-dimensional glow and distortion, richer color
    float extraGlow = 0.0, extraDist = 0.0;
    for (int i = 0; i < numInter; ++i) {
        float ph = phase + inters[i].distance * 2.6 + time * 0.53;
        extraGlow += 0.16 * inters[i].darkMatterDensity * sin(ph) * de_glow;
        extraDist += 0.06 * exp(-inters[i].distance) * cos(ph);
    }
    color += vec3(0.13, 0.18 + 0.13 * dimFrac, 0.34 + 0.09 * de_glow) * extraGlow;
    lensing += extraDist;

    // Collapse effect as rich infrared/bronze
    color += vec3(0.54, 0.19 + 0.05 * dimFrac, 0.08 + 0.07 * coll_frac) * coll_frac * (0.28 + 0.33 * sin(time));

    // Metallic shine
    color += specular * fresnel * 0.54;
    color *= metallicTint * (0.93 + 0.07 * sin(phase) * de_glow);

    // Dimension-based tint using HSV-inspired curve
    float h = mix(0.0, 0.86, dimFrac); // 0 = white, 0.86 = purple-pink
    float s = 0.1 + 0.5 * dimFrac;
    float v = 1.0;
    vec3 dimTint = mix(
        vec3(1.0, 1.0 - 0.4 * s, 1.0 - 0.7 * s),
        vec3(1.0 - 0.23 * h, 0.67 + 0.18 * h, 1.0),
        dimFrac
    );
    color *= dimTint;

    // Alpha calculation, stronger dependency on potential
    float alpha2 = clamp(0.68 + 0.33 * pow(potential / max(observable, 1e-15), 0.93), 0.44, 1.0);

    // Final energy-conservation mapping
    color = mix(color, normalize(color) * length(color) * 0.97, 0.41 + 0.15 * de_glow);
    color = clamp(color, 0.0, 1.0);
    outColor = vec4(color, alpha2);
}