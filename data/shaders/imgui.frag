#version 450 core
#extension GL_EXT_nonuniform_qualifier : enable

layout(set=0, binding=0) uniform sampler2D textures[];
layout(push_constant) uniform uPushConsts
{
    layout(offset = 16) uint texId;
} pc;

layout(location = 0) in struct {
    vec4 Color;
    vec2 UV;
} In;

layout(location = 0) out vec4 fColor;

void main()
{
    fColor = In.Color * texture(textures[pc.texId], In.UV.st);
}
