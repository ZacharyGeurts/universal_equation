#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require

struct RayPayload {
    vec3 color;
    float alpha;
    int depth;
};

struct HitInfo {
    vec3 hitPoint;
    vec3 normal;
    vec3 color;
    float ior;
    float transparency;
};

layout(set = 0, binding = 1) readonly buffer MaterialBuffer {
    vec4 colors[]; // RGB + padding
} materials;

layout(set = 0, binding = 2) readonly buffer IORBuffer {
    float iors[];
} iors;

layout(set = 0, binding = 3) readonly buffer TransparencyBuffer {
    float transparencies[];
} transparencies;

vec3 refract(vec3 incident, vec3 normal, float eta) {
    float cosI = -dot(incident, normal);
    float sinT2 = eta * eta * (1.0 - cosI * cosI);
    if (sinT2 > 1.0) return vec3(0.0);
    float cosT = sqrt(1.0 - sinT2);
    return eta * incident + (eta * cosI - cosT) * normal;
}

float fresnelSchlick(float cosTheta, float ior) {
    float r0 = pow((1.0 - ior) / (1.0 + ior), 2.0);
    return r0 + (1.0 - r0) * pow(1.0 - cosTheta, 5.0);
}