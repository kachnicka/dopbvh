#ifndef RAY_COMMON_GLSL
#define RAY_COMMON_GLSL

#define M_PI 3.1415926535897932384626433832795f
#define M_TWO_PI 6.283185307179586476925286766559f
#define M_PI_INV 0.31830988618379067153776752674503f
#define M_TWO_PI_INV 0.15915494309189533576888376337251f
#define INFINITY 1e32
#define EPSILON 1e-4

struct PathPayload
{
    uint seed;
    float hitT;
    int primitiveID;
    int instanceID;
    int instanceIndex;
    vec2 baryCoord;
    mat4x3 objectToWorld;
    mat4x3 worldToObject;
};

struct ShadowHitPayload
{
    uint seed;
    bool isHit;
};

struct RayKHR
{
    vec3 origin;
    vec3 direction;
};

struct hitPayload
{
    vec3 hitValue;
};

vec3 OffsetRay(in vec3 p, in vec3 n)
{
  const float intScale   = 256.f;
  const float floatScale = 1.f / 65536.f;
  const float origin     = 1.f / 32.f;

    ivec3 of_i = ivec3(intScale * n.x, intScale * n.y, intScale * n.z);

    vec3 p_i = vec3(intBitsToFloat(floatBitsToInt(p.x) + ((p.x < 0) ? -of_i.x : of_i.x)),
        intBitsToFloat(floatBitsToInt(p.y) + ((p.y < 0) ? -of_i.y : of_i.y)),
        intBitsToFloat(floatBitsToInt(p.z) + ((p.z < 0) ? -of_i.z : of_i.z)));

    return vec3(abs(p.x) < origin ? p.x + floatScale * n.x : p_i.x,  //
        abs(p.y) < origin ? p.y + floatScale * n.y : p_i.y,  //
        abs(p.z) < origin ? p.z + floatScale * n.z : p_i.z);
}

vec3 SampleHemisphereCosineWorldSpace(in float r1, in float r2, in vec3 n)
{
    const float factor = 0.9999f;
    const float a = (1.f - 2.f * r1) * factor;
    const float b = sqrt(1.f - a * a) * factor;
    const float phi = M_TWO_PI * r2;

    return normalize(n + vec3(b * cos(phi), b * sin(phi), a));
}

#endif // RAY_COMMON_GLSL