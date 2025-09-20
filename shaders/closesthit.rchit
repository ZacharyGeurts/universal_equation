#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require

// Push constants
layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 proj;
    vec3 baseColor;
    float value;
    float dimension;
    float wavePhase;
    float cycleProgress;
    float darkMatter;
    float darkEnergy;
    vec3 lightColor;
} push;

// Ray payload
layout(location = 0) rayPayloadInNV vec3 rayPayload;

// Hit attributes (no layout qualifier under NV)
hitAttributeNV vec3 attribs;

// Geometry data (assuming vertex buffer with position and normal)
layout(binding = 2, set = 0) buffer Vertices {
    vec3 positions[];
} vertices;
layout(binding = 3, set = 0) buffer Normals {
    vec3 normals[];
} normals;
layout(binding = 4, set = 0) buffer Indices {
    uint indices[];
} indices;

void main() {
    // Get triangle indices
    ivec3 ind = ivec3(indices.indices[3 * gl_PrimitiveID + 0], 
                      indices.indices[3 * gl_PrimitiveID + 1], 
                      indices.indices[3 * gl_PrimitiveID + 2]);
    
    // Barycentric coordinates
    vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    
    // Interpolate normal
    vec3 normal = normalize(normals.normals[ind.x] * barycentrics.x +
                           normals.normals[ind.y] * barycentrics.y +
                           normals.normals[ind.z] * barycentrics.z);
    
    // Transform normal to world space
    normal = mat3(push.model) * normal;
    
    // Simple directional light
    vec3 lightDir = normalize(vec3(0.0, 1.0, 1.0));
    float diff = max(dot(normal, lightDir), 0.0);
    
    // Ambient and diffuse lighting
    vec3 ambient = 0.2 * push.lightColor;
    vec3 diffuse = diff * push.lightColor;
    
    // Modulate base color with darkMatter and darkEnergy
    vec3 color = push.baseColor * (1.0 - push.darkMatter * 0.5) * (1.0 + push.darkEnergy * 0.3);
    
    // Apply dimension-based scaling
    color *= clamp(push.dimension * 0.5, 0.1, 1.0);
    
    // Apply wave modulation
    float modulation = sin(push.wavePhase + push.cycleProgress * 6.28318530718) * push.value * 0.1 + 1.0;
    color *= modulation;
    
    // Combine lighting and color
    rayPayload = (ambient + diffuse) * color;
}