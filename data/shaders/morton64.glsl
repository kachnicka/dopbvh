#ifndef MORTON64_GLSL
#define MORTON64_GLSL 1

const uint MORTON_SCALE_TO_U64 = (1u << 20) - 1;

uint64_t mortonCode64_part(in uint a)
{
    uint64_t x = uint64_t(a) & 2097151L;             // 0x1fffff,
    x = (x | x << 32)        & 8725724278095871L;    // 0x1f00000000ffff,
    x = (x | x << 16)        & 8725728556220671L;    // 0x1f0000ff0000ff,
    x = (x | x << 8)         & 1157144660301377551L; // 0x100f00f00f00f00f,
    x = (x | x << 4)         & 1207822528635744451L; // 0x10c30c30c30c30c3,
    x = (x | x << 2)         & 1317624576693539401L; // 0x1249249249249249
    return x;
}

uint64_t mortonCode64(in uvec3 p)
{
    return mortonCode64_part(p.x) | (mortonCode64_part(p.y) << 1) | (mortonCode64_part(p.z) << 2);
}

uint64_t mortonCode64(in vec3 p)
{
    return mortonCode64(uvec3(p * MORTON_SCALE_TO_U64));
}

#endif