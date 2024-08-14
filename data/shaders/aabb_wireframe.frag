#version 450 core

layout(location = 0) in vec3 bary;

layout(location = 0) out vec4 outColor;

void main()
{
    if (bary.y > .0f && bary.z > .0f)
        discard;
    else
        outColor = vec4(.9f, .9f, .9f, 1.f);
}
