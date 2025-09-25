// shaders/closesthit.rchit
#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable

// Hit attributes (fixed: no layout qualifier)
hitAttributeEXT vec2 attribs;

// Material buffer
struct Material {
    vec3 albedo;
    float opacity;
};
layout(binding = 3, set = 0) buffer Materials {
    Material materials[];
} materialBuffer;

// Payload
layout(location = 0) rayPayloadInEXT vec3 hitValue;

void main() {
    // Get material
    Material mat = materialBuffer.materials[gl_InstanceCustomIndexEXT];

    // Set payload based on material albedo
    hitValue = mat.albedo;
}