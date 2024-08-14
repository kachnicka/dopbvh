#version 450 core
#extension GL_GOOGLE_include_directive : enable

#include "random.glsl"

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;

layout(push_constant) uniform uPushConstant {
    uint geometryBasedColorSeed;
} pc;

layout (set = 0, binding = 0) uniform Camera {
    mat4 view;
    mat4 projection;
    mat4 viewInv;
    mat4 projectionInv;
} camera;

layout (set = 1, binding = 0) uniform Transform {
    mat4 toWorld;
    mat4 toLocal;
} transform;

layout (location = 0) out flat vec3 color;
layout (location = 1) out struct {
    vec3 worldPos;
    vec3 worldNormal;
    vec3 worldPosCamera;
} Out;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    const mat4 vp = camera.projection * camera.view;
    gl_Position = (vp * transform.toWorld) * vec4(inPosition, 1.f);

    if (pc.geometryBasedColorSeed != -1) {
        uint seed = tea(pc.geometryBasedColorSeed, 0);
        color = vec3(rnd(seed), rnd(seed), rnd(seed));
    }
    else
        color = vec3(.8f);

    Out.worldPos = (transform.toWorld * vec4(inPosition, 1.f)).xyz;
    Out.worldNormal = (transform.toLocal * vec4(inNormal, 0.f)).xyz;
    Out.worldPosCamera = camera.viewInv[3].xyz;
}
