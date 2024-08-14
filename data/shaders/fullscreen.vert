#version 450 core

layout(location = 0) out struct {
    vec2 UV;
} Out;

out gl_PerVertex {
    vec4 gl_Position;
};

void main()
{
    Out.UV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(Out.UV * 2.0f - 1.0f, 0.0f, 1.0f);
}
