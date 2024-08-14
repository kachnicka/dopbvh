#ifndef BV_OBB_GLSL
#define BV_OBB_GLSL

float distancePointToEdge(in vec3 point, in vec3 edgePoint, in vec3 edgeDir)
{
    vec3 u = point - edgePoint;
    float t = dot(edgeDir, u);
    float distSq = dot(edgeDir, edgeDir);
    return dot(u, u) - t * t / distSq;
}

float obbCost(in vec3 dim)
{
    dim = abs(dim);
    return dim.x * dim.y + dim.y * dim.z + dim.z * dim.x;
}

float extremalPointsDistance(in vec3[14] points, in vec3 normalizedAxis)
{
    float minDist = dot(points[0], normalizedAxis);
    float maxDist = minDist;
    for (int i = 1; i < 14; ++i) {
        float dist = dot(points[i], normalizedAxis);
        minDist = min(minDist, dist);
        maxDist = max(maxDist, dist);
    }
    return maxDist - minDist;
}

vec2 extremalPointsMinMax(in vec3[14] points, in vec3 normalizedAxis)
{
    float minDist = dot(points[0], normalizedAxis);
    float maxDist = minDist;
    float dist = dot(points[1], normalizedAxis);
    minDist = min(minDist, dist);
    maxDist = max(maxDist, dist);

    dist = dot(points[2], normalizedAxis);
    minDist = min(minDist, dist);
    maxDist = max(maxDist, dist);

    dist = dot(points[3], normalizedAxis);
    minDist = min(minDist, dist);
    maxDist = max(maxDist, dist);

    dist = dot(points[4], normalizedAxis);
    minDist = min(minDist, dist);
    maxDist = max(maxDist, dist);

    dist = dot(points[5], normalizedAxis);
    minDist = min(minDist, dist);
    maxDist = max(maxDist, dist);

    dist = dot(points[6], normalizedAxis);
    minDist = min(minDist, dist);
    maxDist = max(maxDist, dist);

    dist = dot(points[7], normalizedAxis);
    minDist = min(minDist, dist);
    maxDist = max(maxDist, dist);

    dist = dot(points[8], normalizedAxis);
    minDist = min(minDist, dist);
    maxDist = max(maxDist, dist);

    dist = dot(points[9], normalizedAxis);
    minDist = min(minDist, dist);
    maxDist = max(maxDist, dist);

    dist = dot(points[10], normalizedAxis);
    minDist = min(minDist, dist);
    maxDist = max(maxDist, dist);

    dist = dot(points[11], normalizedAxis);
    minDist = min(minDist, dist);
    maxDist = max(maxDist, dist);

    dist = dot(points[12], normalizedAxis);
    minDist = min(minDist, dist);
    maxDist = max(maxDist, dist);

    dist = dot(points[13], normalizedAxis);
    minDist = min(minDist, dist);
    maxDist = max(maxDist, dist);

    return vec2(minDist, maxDist);
}

#define MIN_MAX_DIST_AND_ID(point_id) \
    dist = dot(points[point_id], normalizedAxis); \
    mDist.x = min(mDist.x, dist); \
    mDist.y = max(mDist.y, dist); \
    if (dist == mDist.x) idx.x = point_id; \
    if (dist == mDist.y) idx.y = point_id;


float extremalPointsDistance(in vec3[14] points, in vec3 normalizedAxis, out ivec2 idx, out vec2 mDist)
{
    mDist.x = dot(points[0], normalizedAxis);
    mDist.y = mDist.x;
    idx = ivec2(0, 0);

    float dist;
    dist = dot(points[1], normalizedAxis);
    mDist.x = min(mDist.x, dist);
    mDist.y = max(mDist.y, dist);
    if (dist == mDist.x) idx.x = 1;
    if (dist == mDist.y) idx.y = 1;

    dist = dot(points[2], normalizedAxis);
    mDist.x = min(mDist.x, dist);
    mDist.y = max(mDist.y, dist);
    if (dist == mDist.x) idx.x = 2;
    if (dist == mDist.y) idx.y = 2;

    dist = dot(points[3], normalizedAxis);
    mDist.x = min(mDist.x, dist);
    mDist.y = max(mDist.y, dist);
    if (dist == mDist.x) idx.x = 3;
    if (dist == mDist.y) idx.y = 3;

    dist = dot(points[4], normalizedAxis);
    mDist.x = min(mDist.x, dist);
    mDist.y = max(mDist.y, dist);
    if (dist == mDist.x) idx.x = 4;
    if (dist == mDist.y) idx.y = 4;

    dist = dot(points[5], normalizedAxis);
    mDist.x = min(mDist.x, dist);
    mDist.y = max(mDist.y, dist);
    if (dist == mDist.x) idx.x = 5;
    if (dist == mDist.y) idx.y = 5;

    dist = dot(points[6], normalizedAxis);
    mDist.x = min(mDist.x, dist);
    mDist.y = max(mDist.y, dist);
    if (dist == mDist.x) idx.x = 6;
    if (dist == mDist.y) idx.y = 6;

    dist = dot(points[7], normalizedAxis);
    mDist.x = min(mDist.x, dist);
    mDist.y = max(mDist.y, dist);
    if (dist == mDist.x) idx.x = 7;
    if (dist == mDist.y) idx.y = 7;

    dist = dot(points[8], normalizedAxis);
    mDist.x = min(mDist.x, dist);
    mDist.y = max(mDist.y, dist);
    if (dist == mDist.x) idx.x = 8;
    if (dist == mDist.y) idx.y = 8;

    dist = dot(points[9], normalizedAxis);
    mDist.x = min(mDist.x, dist);
    mDist.y = max(mDist.y, dist);
    if (dist == mDist.x) idx.x = 9;
    if (dist == mDist.y) idx.y = 9;

    dist = dot(points[10], normalizedAxis);
    mDist.x = min(mDist.x, dist);
    mDist.y = max(mDist.y, dist);
    if (dist == mDist.x) idx.x = 10;
    if (dist == mDist.y) idx.y = 10;

    dist = dot(points[11], normalizedAxis);
    mDist.x = min(mDist.x, dist);
    mDist.y = max(mDist.y, dist);
    if (dist == mDist.x) idx.x = 11;
    if (dist == mDist.y) idx.y = 11;

    dist = dot(points[12], normalizedAxis);
    mDist.x = min(mDist.x, dist);
    mDist.y = max(mDist.y, dist);
    if (dist == mDist.x) idx.x = 12;
    if (dist == mDist.y) idx.y = 12;

    dist = dot(points[13], normalizedAxis);
    mDist.x = min(mDist.x, dist);
    mDist.y = max(mDist.y, dist);
    if (dist == mDist.x) idx.x = 13;
    if (dist == mDist.y) idx.y = 13;

    return mDist.y - mDist.x;
}

float obbFromTriangle(in vec3[14] points, in vec3 e0, in vec3 e1, in vec3 e2, in vec3 n)
{
    float area = 0.f;

    vec3 obbAxis = normalize(cross(n, e0));
    vec3 obbDim = vec3(extremalPointsDistance(points, e0), extremalPointsDistance(points, obbAxis), extremalPointsDistance(points, n));
    area = obbCost(obbDim);

    obbAxis = normalize(cross(n, e1));
    obbDim.xy = vec2(extremalPointsDistance(points, e1), extremalPointsDistance(points, obbAxis));
    area = min(area, obbCost(obbDim));

    obbAxis = normalize(cross(n, e2));
    obbDim.xy = vec2(extremalPointsDistance(points, e2), extremalPointsDistance(points, obbAxis));
    area = min(area, obbCost(obbDim));

    return area;
}

struct OBB {
    vec3 b0;
    vec3 b1;
    vec3 b2;
    vec3 min;
    vec3 max;
};

struct iOBB {
    vec3 b0;
    vec3 b1;
    vec3 b2;
    ivec3 min;
    ivec3 max;
};

void refitObb(inout OBB obb, in vec3 v)
{
    vec3 proj = vec3(dot(v, obb.b0), dot(v, obb.b1), dot(v, obb.b2));
    obb.min = min(obb.min, proj);
    obb.max = max(obb.max, proj);
}

void refitObb(inout iOBB obb, in vec3 v)
{
    ivec3 proj = floatBitsToInt(vec3(dot(v, obb.b0), dot(v, obb.b1), dot(v, obb.b2)));
    obb.min = min(obb.min, proj);
    obb.max = max(obb.max, proj);
}

void obbFromTriangle(inout OBB obb, inout float minObbCost, in vec3[14] points, in vec3 e0, in vec3 e1, in vec3 e2, in vec3 n)
{
    vec3 obbAxis = normalize(cross(n, e0));
    vec2 b0mm = extremalPointsMinMax(points, e0);
    vec2 b1mm = extremalPointsMinMax(points, obbAxis);
    const vec2 b2mm = extremalPointsMinMax(points, n);
    vec3 obbDim = vec3(b0mm.y - b0mm.x, b1mm.y - b1mm.x, b2mm.y - b2mm.x);
    float cost = obbCost(obbDim);
    if (cost < minObbCost) {
        minObbCost = cost;
        obb.b0 = e0;
        obb.b1 = obbAxis;
        obb.b2 = n;
        obb.min = vec3(b0mm.x, b1mm.x, b2mm.x);
        obb.max = vec3(b0mm.y, b1mm.y, b2mm.y);
    }

    obbAxis = normalize(cross(n, e1));
    b0mm = extremalPointsMinMax(points, e1);
    b1mm = extremalPointsMinMax(points, obbAxis);
    obbDim.xy = vec2(b0mm.y - b0mm.x, b1mm.y - b1mm.x);
    cost = obbCost(obbDim);
    if (cost < minObbCost) {
        minObbCost = cost;
        obb.b0 = e1;
        obb.b1 = obbAxis;
        obb.b2 = n;
        obb.min = vec3(b0mm.x, b1mm.x, b2mm.x);
        obb.max = vec3(b0mm.y, b1mm.y, b2mm.y);
    }

    obbAxis = normalize(cross(n, e2));
    b0mm = extremalPointsMinMax(points, e2);
    b1mm = extremalPointsMinMax(points, obbAxis);
    obbDim.xy = vec2(b0mm.y - b0mm.x, b1mm.y - b1mm.x);
    cost = obbCost(obbDim);
    if (cost < minObbCost) {
        minObbCost = cost;
        obb.b0 = e2;
        obb.b1 = obbAxis;
        obb.b2 = n;
        obb.min = vec3(b0mm.x, b1mm.x, b2mm.x);
        obb.max = vec3(b0mm.y, b1mm.y, b2mm.y);
    }
}

// expects obb initialized to aabb of the node
void obbByDiTO14(inout OBB obb, in vec3[14] points)
{
    float minObbCost = obbCost(obb.max - obb.min);

    // find the ditetrahedron base triangle
    // 1. find the two points that are furthest apart
    ivec3 baseTriangleIdx = ivec3(0, 1, 0);
    vec3 dVec = points[1] - points[0];
    float maxDistSq = dot(dVec, dVec);
    for (int i = 1; i < 7; ++i) {
        dVec = points[i * 2 + 1] - points[i * 2 + 0];
        float distSq = dot(dVec, dVec);
        if (distSq > maxDistSq) {
            maxDistSq = distSq;
            baseTriangleIdx.x = i * 2 + 0;
            baseTriangleIdx.y = i * 2 + 1;
        }
    }
    const vec3 p0 = points[baseTriangleIdx.x];
    const vec3 p1 = points[baseTriangleIdx.y];
    const vec3 e0 = normalize(p1 - p0);
    // 2. find the point furthest from the line between the two points
    maxDistSq = distancePointToEdge(points[0], p0, e0);
    for (int i = 1; i < 14; ++i) {
        float distSq = distancePointToEdge(points[i], p0, e0);
        if (distSq > maxDistSq) {
            maxDistSq = distSq;
            baseTriangleIdx.z = i;
        }
    }
    const vec3 p2 = points[baseTriangleIdx.z];
    const vec3 e1 = normalize(p2 - p0);
    const vec3 e2 = normalize(p2 - p1);
    const vec3 normal = normalize(cross(e0, e1));

    // 3. find the top and bottom tetrahedron points
    ivec2 ditPointsIdx;
    vec2 ditPointsDist;
    float ditDist = extremalPointsDistance(points, normal, ditPointsIdx, ditPointsDist);

    // form OBB axes from each triangle edge, normal and corresponding perpendicular axis
    obbFromTriangle(obb, minObbCost, points, e0, e1, e2, normal);

    const vec3 q0 = points[ditPointsIdx.x];
    const vec3 q1 = points[ditPointsIdx.y];

    //     test all triangles of the ditetrahedron for better OBB
    if (abs(ditPointsDist.x) > .01f)
    {
        vec3 m0 = normalize(q0 - p0);
        vec3 m1 = normalize(q0 - p1);
        vec3 m2 = normalize(q0 - p2);
        vec3 n0 = normalize(cross(m0, m1));
        vec3 n1 = normalize(cross(m1, m2));
        vec3 n2 = normalize(cross(m2, m0));

        obbFromTriangle(obb, minObbCost, points, e0, m0, m1, n0);
        obbFromTriangle(obb, minObbCost, points, e2, m1, m2, n1);
        obbFromTriangle(obb, minObbCost, points, e1, m2, m0, n2);
    }
    if (abs(ditPointsDist.y) > .01f)
    {
        vec3 m0 = normalize(q1 - p0);
        vec3 m1 = normalize(q1 - p1);
        vec3 m2 = normalize(q1 - p2);
        vec3 n0 = normalize(cross(m0, m1));
        vec3 n1 = normalize(cross(m1, m2));
        vec3 n2 = normalize(cross(m2, m0));

        obbFromTriangle(obb, minObbCost, points, e0, m0, m1, n0);
        obbFromTriangle(obb, minObbCost, points, e2, m1, m2, n1);
        obbFromTriangle(obb, minObbCost, points, e1, m2, m0, n2);
    }
}

vec3[14] ditoDummy()
{
    vec3[14] points;
    float dop[14] = dopInitWithPoints(vec3(-BIG_FLOAT), points);
    bvFitWithPoints(dop, vec3(BIG_FLOAT), points);
    return points;
}

float bvArea(in mat4x3 m)
{
    mat3 m0 = mat3(m);
    m0 = inverse(m0);
    vec3 scale = vec3(length(m0[0]), length(m0[1]), length(m0[2]));
    return (scale.x * scale.y + scale.y * scale.z + scale.z * scale.x) * 2.f;
}
#endif