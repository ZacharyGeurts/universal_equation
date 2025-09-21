#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

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

layout(buffer_reference, scalar, buffer_reference_align = 8) readonly buffer Vertices {
    vec3 vertices[];
};

layout(buffer_reference, scalar, buffer_reference_align = 8) readonly buffer Indices {
    uvec3 indices[];
};

layout(buffer_reference, scalar, buffer_reference_align = 8) readonly buffer Normals {
    vec3 normals[];
};

layout(set = 0, binding = 0) uniform accelerationStructureEXT topLevelAS;

layout(set = 0, binding = 1) readonly buffer MaterialBuffer {
    vec4 colors[]; // RGB + padding
} materials;

layout(set = 0, binding = 2) readonly buffer IORBuffer {
    float iors[];
} iors;

layout(set = 0, binding = 3) readonly buffer TransparencyBuffer {
    float transparencies[];
} transparencies;

layout(set = 0, binding = 4, scalar) buffer SceneDesc {
    uint64_t vertexAddr;
    uint64_t indexAddr;
    uint64_t normalAddr;
} scene;

layout(set = 0, binding = 5) uniform UBO {
    mat4 viewInverse;
    mat4 projInverse;
    vec3 lightPos;
    vec3 lightColor;
} ubo;

layout(set = 0, binding = 6, rgba32f) uniform image2D outputImage;

layout(location = 0) rayPayloadEXT RayPayload payload;
layout(location = 1) rayPayloadEXT bool isShadowed;
layout(location = 0) hitAttributeEXT vec2 baryCoord;

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

// Ray Generation Shader
void mainRayGen() {
    uvec3 launchID = gl_LaunchIDEXT;
    uvec3 launchSize = gl_LaunchSizeEXT;
    vec2 pixelCenter = vec2(launchID.xy) + vec2(0.5);
    vec2 uv = pixelCenter / vec2(launchSize.xy);
    vec2 d = uv * 2.0 - 1.0;

    vec4 origin = ubo.viewInverse * vec4(0.0, 0.0, 0.0, 1.0);
    vec4 target = ubo.projInverse * vec4(d.x, d.y, 1.0, 1.0);
    vec4 direction = ubo.viewInverse * vec4(normalize(target.xyz), 0.0);

    payload.depth = 0;
    payload.color = vec3(0.0);
    payload.alpha = 0.0;

    traceRayEXT(topLevelAS, gl_RayFlagsNoneEXT, 0xff, 0, 0, 0, origin.xyz, 0.001, direction.xyz, 10000.0, 0);

    imageStore(outputImage, ivec2(launchID.xy), vec4(payload.color, payload.alpha));
}

// Miss Shader for Regular Rays
void mainMiss() {
    payload.color = vec3(0.2, 0.4, 0.6); // Simple sky background
    payload.alpha = 1.0;
}

// Miss Shader for Shadow Rays
void mainShadowMiss() {
    isShadowed = false;
}

// Closest Hit Shader
void mainClosestHit() {
    if (payload.depth > 5) {
        payload.color = vec3(0.0);
        payload.alpha = 0.0;
        return;
    }

    // Memory-safe access: Assume matID is within bounds as per scene setup
    int matID = gl_InstanceCustomIndexEXT;

    vec3 albedo = materials.colors[matID].rgb;
    float ior = iors.iors[matID];
    float transparency = transparencies.transparencies[matID];

    vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

    // Fetch normal with barycentric interpolation
    Vertices verts = Vertices(scene.vertexAddr);
    Indices inds = Indices(scene.indexAddr);
    Normals norms = Normals(scene.normalAddr);

    // Memory-safe: Assume gl_PrimitiveID < array size
    uvec3 idx = inds.indices[gl_PrimitiveID];

    vec3 bary = vec3(1.0 - baryCoord.x - baryCoord.y, baryCoord.x, baryCoord.y);

    vec3 objNormal = norms.normals[idx.x] * bary.x + norms.normals[idx.y] * bary.y + norms.normals[idx.z] * bary.z;

    mat3 normalMatrix = transpose(inverse(mat3(gl_ObjectToWorldEXT)));

    vec3 normal = normalize(normalMatrix * objNormal);

    vec3 I = gl_WorldRayDirectionEXT;

    bool inside = dot(I, normal) > 0.0;
    vec3 orientedNormal = inside ? -normal : normal;

    float cosTheta = -dot(I, orientedNormal);

    float eta = inside ? ior : (1.0 / ior);

    float reflectAmount = fresnelSchlick(cosTheta, ior);

    vec3 reflectDir = reflect(I, orientedNormal);
    vec3 refractDir = refract(I, orientedNormal, eta);

    if (dot(refractDir, refractDir) < 1e-5) {
        reflectAmount = 1.0; // Total internal reflection
    }

    int currentDepth = payload.depth;

    vec3 transmitColor = vec3(0.0);

    float transmitScale = (transparency > 0.0) ? transparency : 1.0;
    vec3 reflectTint = (transparency > 0.0) ? vec3(1.0) : albedo;
    vec3 transmitTint = albedo;

    if (transparency > 0.0) {
        // Trace refraction ray
        payload.color = vec3(0.0);
        payload.alpha = 0.0;
        payload.depth = currentDepth + 1;
        traceRayEXT(topLevelAS, gl_RayFlagsNoneEXT, 0xff, 0, 0, 0, hitPoint, 0.001, refractDir, 10000.0, 0);
        transmitColor = payload.color;
    } else {
        // Compute local diffuse illumination
        vec3 L = normalize(ubo.lightPos - hitPoint);
        float cosNL = max(0.0, dot(orientedNormal, L));

        isShadowed = true;
        float lightDist = length(ubo.lightPos - hitPoint);
        traceRayEXT(topLevelAS, gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT, 0xff, 0, 0, 1, hitPoint, 0.001, L, lightDist, 1);

        vec3 localColor = vec3(0.0);
        if (!isShadowed) {
            localColor = (ubo.lightColor * cosNL) / 3.1415926538;
        }
        transmitColor = localColor;
    }

    // Trace reflection ray
    payload.color = vec3(0.0);
    payload.alpha = 0.0;
    payload.depth = currentDepth + 1;
    traceRayEXT(topLevelAS, gl_RayFlagsNoneEXT, 0xff, 0, 0, 0, hitPoint, 0.001, reflectDir, 10000.0, 0);
    vec3 reflectColor = payload.color;

    // Combine
    payload.color = (reflectColor * reflectTint * reflectAmount) + (transmitColor * transmitTint * (1.0 - reflectAmount) * transmitScale);
    payload.alpha = 1.0;
}