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

// Wave-based noise for music visualizer effect
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float noise(vec2 uv) {
    float n = 0.0, amp = 0.5, f = 1.0;
    for (int i = 0; i < 4; ++i) {
        n += amp * hash(uv * f + inCycleProgress * 0.2 * float(i+1));
        f *= 2.0;
        amp *= 0.5;
    }
    return n;
}

// Wave visualizer effect mimicking audio bars
float waveVisualizer(vec2 uv, float phase) {
    float wave = sin(uv.x * 10.0 + phase * 2.0) * cos(uv.y * 5.0 + phase * 1.5);
    float bars = abs(sin(uv.y * 20.0 + phase)) * (0.5 + 0.5 * darkMatterPulse);
    return mix(wave, bars, 0.5 + 0.3 * sin(phase));
}

// Vibrant color pulses for dark energy
vec3 darkEnergyEmission(float darkEnergy, float phase, float dimFrac) {
    float e = 0.8 + 0.6 * darkEnergy * abs(sin(phase * 1.5));
    float r = 0.4 * e * (1.0 - dimFrac);
    float g = 0.5 * e * dimFrac;
    float b = 0.7 + 0.3 * e;
    return vec3(r, g, b);
}

// Dynamic bar-like specular highlights
vec3 specularReflection(float shininess, float phase, vec2 uv, float anisotropy) {
    float spec = pow(abs(sin(phase + uv.y * 15.0 * anisotropy)), shininess);
    return vec3(0.9, 0.7, 0.6) * spec * (0.7 + 0.3 * darkEnergyGlow);
}

// Chart-like edge enhancement
float chartEdge(vec2 uv, float phase, float dimFrac) {
    float edge = abs(sin(uv.y * 25.0 + phase));
    return mix(0.3, 1.0, edge) * (0.8 + 0.2 * dimFrac);
}

float computeDarkMatterDensity(int dim, float invMaxDim, float darkMatterStrength) {
    float density = darkMatterStrength * (1.0 + float(dim) * invMaxDim);
    if (dim > 3) density *= (1.0 + 0.15 * float(dim - 3));
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
    const float darkMatterStrength = 0.3;
    const float darkEnergyStrength = 0.7;
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

    float dm_pulse = pow(clamp(darkMatter / 0.7 + darkMatterPulse * 0.6, 0.0, 2.0), 0.8);
    float de_glow = pow(clamp((darkEnergy - 0.8) / 1.0 + darkEnergyGlow * 0.6, 0.0, 2.0), 0.9);
    float coll_frac = (totalInfluence > 0.0) ? clamp(coll / totalInfluence, 0.0, 1.0) : 0.0;

    vec3 color = inBaseColor;
    float time = inCycleProgress * pi * 2.0;
    vec2 uv = gl_FragCoord.xy * 0.005;
    float n = 0.15 * noise(uv * (0.9 + 0.6 * dm_pulse + 0.4 * sin(time)));
    float phase = inWavePhase + time;
    float dimFrac = float(currentDimension - 1) / 8.0;

    float visualizer = waveVisualizer(uv, phase) * (1.0 + 0.1 * de_glow);
    vec3 darkEnergyEmit = darkEnergyEmission(de_glow, phase, dimFrac);

    float shininess = 15.0 + 20.0 * pow(dm_pulse, 1.2);
    float anisotropy = 0.8 + 0.4 * sin(phase + observable);
    vec3 specular = specularReflection(shininess, phase, uv, anisotropy);

    float edge = chartEdge(uv, phase, dimFrac);

    vec3 chartTint = vec3(
        0.8 + 0.2 * dm_pulse,
        0.7 + 0.15 * coll_frac,
        0.6 + 0.3 * de_glow
    );

    float intensity = inValue * pow(observable / 3.0, 0.9) * (0.6 + 0.4 * cos(phase + n));
    intensity *= visualizer * (pow(max(potential, 0.0) / 2.0 + 0.5, 0.9));

    color *= intensity;
    color += darkEnergyEmit * (0.25 + 0.1 * dm_pulse);

    float extraGlow = 0.0, extraDist = 0.0;
    for (int i = 0; i < numInter; ++i) {
        float ph = phase + inters[i].distance * 2.5 + time * 0.6;
        extraGlow += 0.2 * inters[i].darkMatterDensity * sin(ph) * de_glow;
        extraDist += 0.08 * exp(-inters[i].distance) * cos(ph);
    }
    color += vec3(0.15, 0.2 + 0.1 * dimFrac, 0.3 + 0.1 * de_glow) * extraGlow;
    visualizer += extraDist;

    color += vec3(0.5, 0.2 + 0.06 * dimFrac, 0.1 + 0.08 * coll_frac) * coll_frac * (0.3 + 0.4 * sin(time));

    color += specular * edge * 0.6;
    color *= chartTint * (0.9 + 0.1 * sin(phase) * de_glow);

    float h = mix(0.0, 0.9, dimFrac);
    float s = 0.2 + 0.4 * dimFrac;
    float v = 1.0;
    vec3 dimTint = mix(
        vec3(1.0, 1.0 - 0.3 * s, 1.0 - 0.6 * s),
        vec3(1.0 - 0.2 * h, 0.7 + 0.2 * h, 1.0),
        dimFrac
    );
    color *= dimTint;

    float alpha2 = clamp(0.7 + 0.3 * pow(potential / max(observable, 1e-15), 0.9), 0.5, 1.0);

    color = mix(color, normalize(color) * length(color) * 0.95, 0.4 + 0.2 * de_glow);
    color = clamp(color, 0.0, 1.0);
    outColor = vec4(color, alpha2);
}