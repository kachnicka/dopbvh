#ifndef BV_AABB_GLSL
#define BV_AABB_GLSL

struct Aabb {
    vec3 min;
    vec3 max;
};

void bvFit(inout Aabb aabb, in vec3 v)
{
    aabb.min = min(aabb.min, v);
    aabb.max = max(aabb.max, v);
}

void bvFit(inout Aabb aabb, in Aabb aabbToFit)
{
    bvFit(aabb, aabbToFit.min);
    bvFit(aabb, aabbToFit.max);
}

Aabb overlapAabb(in Aabb a, in Aabb b)
{
    Aabb result;
    result.min = max(a.min, b.min);
    result.max = min(a.max, b.max);

    if (result.min.x > result.max.x || result.min.y > result.max.y || result.min.z > result.max.z) {
        result.min = vec3(0.f);
        result.max = vec3(0.f);
    }
    return result;
}

vec3 aabbCentroid(in Aabb aabb)
{
    return (aabb.min + aabb.max) * .5f;
}

float bvArea(in Aabb aabb)
{
    vec3 d = aabb.max - aabb.min;
    return 2.f * (d.x * d.y + d.x * d.z + d.z * d.y);
}

#define BIG_FLOAT 1e30f

Aabb aabbDummy()
{
    return Aabb(vec3(-BIG_FLOAT), vec3(BIG_FLOAT));
}

struct Aabb_PAD64 {
    vec4 min;
    vec4 max;
    vec4 dynamicUniformPadding[2];
};

vec4 getAabbVertex(in Aabb_PAD64 aabb, in uint index) {
    switch (index) {
        case 0:
            return vec4(aabb.min.x, aabb.min.y, aabb.min.z, 1.0f);
        case 1:
            return vec4(aabb.max.x, aabb.min.y, aabb.min.z, 1.0f);
        case 2:
            return vec4(aabb.max.x, aabb.min.y, aabb.max.z, 1.0f);
        case 3:
            return vec4(aabb.min.x, aabb.min.y, aabb.max.z, 1.0f);

        case 4:
            return vec4(aabb.min.x, aabb.max.y, aabb.min.z, 1.0f);
        case 5:
            return vec4(aabb.max.x, aabb.max.y, aabb.min.z, 1.0f);
        case 6:
            return vec4(aabb.max.x, aabb.max.y, aabb.max.z, 1.0f);
        case 7:
            return vec4(aabb.min.x, aabb.max.y, aabb.max.z, 1.0f);
    }
}

vec4 getAabbVertex(in Aabb aabb, in uint index) {
    switch (index) {
        case 0:
        return vec4(aabb.min.x, aabb.min.y, aabb.min.z, 1.0f);
        case 1:
        return vec4(aabb.max.x, aabb.min.y, aabb.min.z, 1.0f);
        case 2:
        return vec4(aabb.max.x, aabb.min.y, aabb.max.z, 1.0f);
        case 3:
        return vec4(aabb.min.x, aabb.min.y, aabb.max.z, 1.0f);

        case 4:
        return vec4(aabb.min.x, aabb.max.y, aabb.min.z, 1.0f);
        case 5:
        return vec4(aabb.max.x, aabb.max.y, aabb.min.z, 1.0f);
        case 6:
        return vec4(aabb.max.x, aabb.max.y, aabb.max.z, 1.0f);
        case 7:
        return vec4(aabb.min.x, aabb.max.y, aabb.max.z, 1.0f);
    }
}

#endif