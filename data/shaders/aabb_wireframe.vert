#version 450 core
#extension GL_GOOGLE_include_directive: enable

#include "bv_aabb.glsl"
#include "bv_aabb_data.glsl"

layout (set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    mat4 viewInv;
    mat4 projectionInv;
} camera;

layout (set = 1, binding = 0) uniform AabbUniform {
    Aabb_PAD64 data;
} aabb;

layout (set = 1, binding = 1) uniform Transform {
    mat4 toWorld;
    mat4 toLocal;
} transform;

out gl_PerVertex {
    vec4 gl_Position;
};

layout (location = 0) out noperspective vec3 bary;

void main() {
    const mat4 vp = camera.projection * camera.view;
    gl_Position = (vp * transform.toWorld) * getAabbVertex(aabb.data, aabb_indices[gl_InstanceIndex][gl_VertexIndex]);
    bary = aabb_barycentrics[gl_VertexIndex];
}
