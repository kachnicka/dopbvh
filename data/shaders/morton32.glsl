#ifndef MORTON32_GLSL
#define MORTON32_GLSL 1

#extension GL_EXT_shader_explicit_arithmetic_types_int32: require

const uint32_t MORTON_SCALE_TO_U32 = (1u << 10) - 1;

uint32_t mortonCode32_part(in uint a)
{
    uint32_t x = a    & 0x000003ff;
    x = (x | x << 16) & 0x30000ff;
    x = (x | x << 8)  & 0x0300f00f;
    x = (x | x << 4)  & 0x30c30c3;
    x = (x | x << 2)  & 0x9249249;
    return x;
}

uint32_t mortonCode32(in uvec3 p)
{
    return mortonCode32_part(p.x) | (mortonCode32_part(p.y) << 1) | (mortonCode32_part(p.z) << 2);
}

uint32_t mortonCode32(in vec3 p)
{
    return mortonCode32(uvec3(p * MORTON_SCALE_TO_U32));
}

#endif