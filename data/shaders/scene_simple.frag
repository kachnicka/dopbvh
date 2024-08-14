#version 450 core

layout (location = 0) in flat vec3 color;
layout (location = 1) in struct {
    vec3 worldPos;
    vec3 worldNormal;
    vec3 worldPosCamera;
} In;


layout (location = 0) out vec4 outColor;

void main() {
    const vec3 V = normalize(In.worldPosCamera - In.worldPos);
    outColor = vec4(abs(dot(In.worldNormal, V)) * color, 1.f);
}
