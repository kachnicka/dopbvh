#ifndef BV_AABB_DATA_GLSL
#define BV_AABB_DATA_GLSL

uint aabb_indices[12][3] = {
    { 0, 1, 3, },
    { 2, 3, 1, },
    { 1, 5, 2, },
    { 6, 2, 5, },
    { 5, 4, 6, },
    { 7, 6, 4, },
    { 4, 0, 7, },
    { 3, 7, 0, },
    { 4, 5, 0, },
    { 1, 0, 5, },
    { 3, 2, 7, },
    { 6, 7, 2, },
};

vec3 aabb_barycentrics[3] = {
    vec3(1.f, 0.f, 0.f),
    vec3(0.f, 1.f, 0.f),
    vec3(0.f, 0.f, 1.f),
};

#endif