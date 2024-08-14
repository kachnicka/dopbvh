#ifndef BV_DOP14_GLSL
#define BV_DOP14_GLSL

#extension GL_EXT_shader_explicit_arithmetic_types_int8: require

#ifndef BIF_FLOAT
#define BIG_FLOAT 1e30f
#endif

// dop14 with normalized normals
vec3 DOP14_NORMAL[7] = {
    { 1.f, 0.f, 0.f },
    { 0.f, 1.f, 0.f },
    { 0.f, 0.f, 1.f },

    { 0.57735f, 0.57735f, 0.57735f },
    { 0.57735f, 0.57735f, -0.57735f },
    { 0.57735f, -0.57735f, 0.57735f },
    { 0.57735f, -0.57735f, -0.57735f },
};

// dop14 with integer coordinates [-1, 0, 1]
float dot_dop14_n0(in vec3 v) { return v.x; }
float dot_dop14_n1(in vec3 v) { return v.y; }
float dot_dop14_n2(in vec3 v) { return v.z; }
float dot_dop14_n3(in vec3 v) { return (v.x + v.y + v.z); }
float dot_dop14_n4(in vec3 v) { return (v.x + v.y - v.z); }
float dot_dop14_n5(in vec3 v) { return (v.x - v.y + v.z); }
float dot_dop14_n6(in vec3 v) { return (v.x - v.y - v.z); }

float[14] dopInit(in vec3 v)
{
    float dop[14];
    dop[0] = dot_dop14_n0(v);
    dop[1] = dop[0];
    dop[2] = dot_dop14_n1(v);
    dop[3] = dop[2];
    dop[4] = dot_dop14_n2(v);
    dop[5] = dop[4];
    dop[6] = dot_dop14_n3(v);
    dop[7] = dop[6];
    dop[8] = dot_dop14_n4(v);
    dop[9] = dop[8];
    dop[10] = dot_dop14_n5(v);
    dop[11] = dop[10];
    dop[12] = dot_dop14_n6(v);
    dop[13] = dop[12];
    return dop;
}

float[14] dopInit()
{
    float dop[14];
    dop[0] = BIG_FLOAT;
    dop[1] = -BIG_FLOAT;
    dop[2] = BIG_FLOAT;
    dop[3] = -BIG_FLOAT;
    dop[4] = BIG_FLOAT;
    dop[5] = -BIG_FLOAT;
    dop[6] = BIG_FLOAT;
    dop[7] = -BIG_FLOAT;
    dop[8] = BIG_FLOAT;
    dop[9] = -BIG_FLOAT;
    dop[10] = BIG_FLOAT;
    dop[11] = -BIG_FLOAT;
    dop[12] = BIG_FLOAT;
    dop[13] = -BIG_FLOAT;
    return dop;
}

// result of dopFit(vec3(-BIG_FLOAT), vec3(BIG_FLOAT))
float[14] dopDummy()
{
    float dop[14];
    dop[0] = -1e30f;
    dop[1] = 1e30f;
    dop[2] = -1e30f;
    dop[3] = 1e30f;
    dop[4] = -1e30f;
    dop[5] = 1e30f;
    dop[6] = -1.73205e30f;
    dop[7] = 1.73205e30f;
    dop[8] = -5.7735e29f;
    dop[9] = 5.7735e29f;
    dop[10] = -5.7735e29f;
    dop[11] = 5.7735e29f;
    dop[12] = -5.7735e29f;
    dop[13] = 5.7735e29f;

    return dop;
}

void bvFit(inout float dop[14], in vec3 v)
{
    float d;
    d = dot_dop14_n0(v);
    dop[0] = min(dop[0], d);
    dop[1] = max(dop[1], d);
    d = dot_dop14_n1(v);
    dop[2] = min(dop[2], d);
    dop[3] = max(dop[3], d);
    d = dot_dop14_n2(v);
    dop[4] = min(dop[4], d);
    dop[5] = max(dop[5], d);
    d = dot_dop14_n3(v);
    dop[6] = min(dop[6], d);
    dop[7] = max(dop[7], d);
    d = dot_dop14_n4(v);
    dop[8] = min(dop[8], d);
    dop[9] = max(dop[9], d);
    d = dot_dop14_n5(v);
    dop[10] = min(dop[10], d);
    dop[11] = max(dop[11], d);
    d = dot_dop14_n6(v);
    dop[12] = min(dop[12], d);
    dop[13] = max(dop[13], d);
}

float[14] dopInit(in vec3 v0, in vec3 v1, in vec3 v2)
{
    float[14] dop;
    dop[0] = min(dot_dop14_n0(v0), min(dot_dop14_n0(v1), dot_dop14_n0(v2)));
    dop[1] = max(dot_dop14_n0(v0), max(dot_dop14_n0(v1), dot_dop14_n0(v2)));
    dop[2] = min(dot_dop14_n1(v0), min(dot_dop14_n1(v1), dot_dop14_n1(v2)));
    dop[3] = max(dot_dop14_n1(v0), max(dot_dop14_n1(v1), dot_dop14_n1(v2)));
    dop[4] = min(dot_dop14_n2(v0), min(dot_dop14_n2(v1), dot_dop14_n2(v2)));
    dop[5] = max(dot_dop14_n2(v0), max(dot_dop14_n2(v1), dot_dop14_n2(v2)));
    dop[6] = min(dot_dop14_n3(v0), min(dot_dop14_n3(v1), dot_dop14_n3(v2)));
    dop[7] = max(dot_dop14_n3(v0), max(dot_dop14_n3(v1), dot_dop14_n3(v2)));
    dop[8] = min(dot_dop14_n4(v0), min(dot_dop14_n4(v1), dot_dop14_n4(v2)));
    dop[9] = max(dot_dop14_n4(v0), max(dot_dop14_n4(v1), dot_dop14_n4(v2)));
    dop[10] = min(dot_dop14_n5(v0), min(dot_dop14_n5(v1), dot_dop14_n5(v2)));
    dop[11] = max(dot_dop14_n5(v0), max(dot_dop14_n5(v1), dot_dop14_n5(v2)));
    dop[12] = min(dot_dop14_n6(v0), min(dot_dop14_n6(v1), dot_dop14_n6(v2)));
    dop[13] = max(dot_dop14_n6(v0), max(dot_dop14_n6(v1), dot_dop14_n6(v2)));
    return dop;
}

void bvFit(inout float dop[14], in float dopToFit[14])
{
    dop[0] = min(dop[0], dopToFit[0]);
    dop[1] = max(dop[1], dopToFit[1]);
    dop[2] = min(dop[2], dopToFit[2]);
    dop[3] = max(dop[3], dopToFit[3]);
    dop[4] = min(dop[4], dopToFit[4]);
    dop[5] = max(dop[5], dopToFit[5]);
    dop[6] = min(dop[6], dopToFit[6]);
    dop[7] = max(dop[7], dopToFit[7]);
    dop[8] = min(dop[8], dopToFit[8]);
    dop[9] = max(dop[9], dopToFit[9]);
    dop[10] = min(dop[10], dopToFit[10]);
    dop[11] = max(dop[11], dopToFit[11]);
    dop[12] = min(dop[12], dopToFit[12]);
    dop[13] = max(dop[13], dopToFit[13]);
}

float[14] overlapDop(in float dop[14], in float dop1[14])
{
    dop[0] = max(dop[0], dop1[0]);
    dop[2] = max(dop[2], dop1[2]);
    dop[4] = max(dop[4], dop1[4]);
    dop[6] = max(dop[6], dop1[6]);
    dop[8] = max(dop[8], dop1[8]);
    dop[10] = max(dop[10], dop1[10]);
    dop[12] = max(dop[12], dop1[12]);
    dop[1] = min(dop[1], dop1[1]);
    dop[3] = min(dop[3], dop1[3]);
    dop[5] = min(dop[5], dop1[5]);
    dop[7] = min(dop[7], dop1[7]);
    dop[9] = min(dop[9], dop1[9]);
    dop[11] = min(dop[11], dop1[11]);
    dop[13] = min(dop[13], dop1[13]);

    if (dop[0] > dop[1] || dop[2] > dop[3] || dop[4] > dop[5] || dop[6] > dop[7] || dop[8] > dop[9] || dop[10] > dop[11] || dop[12] > dop[13])
        dop = float[14]( 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f );
    return dop;
}

float[14] dopInitWithPoints(in vec3 v, out vec3 points[14])
{
    for (int i = 0; i < 14; ++i)
        points[i] = v;

    float dop[14];
    dop[0] = dot_dop14_n0(v);
    dop[1] = dop[0];
    dop[2] = dot_dop14_n1(v);
    dop[3] = dop[2];
    dop[4] = dot_dop14_n2(v);
    dop[5] = dop[4];
    dop[6] = dot_dop14_n3(v);
    dop[7] = dop[6];
    dop[8] = dot_dop14_n4(v);
    dop[9] = dop[8];
    dop[10] = dot_dop14_n5(v);
    dop[11] = dop[10];
    dop[12] = dot_dop14_n6(v);
    dop[13] = dop[12];
    return dop;
}

void bvFitWithPoints(inout float dop[14], in vec3 v, inout vec3 points[14])
{
    float d;
    d = dot_dop14_n0(v);
    dop[0] = min(dop[0], d);
    if (d == dop[0]) points[0] = v;
    dop[1] = max(dop[1], d);
    if (d == dop[1]) points[1] = v;

    d = dot_dop14_n1(v);
    dop[2] = min(dop[2], d);
    if (d == dop[2]) points[2] = v;
    dop[3] = max(dop[3], d);
    if (d == dop[3]) points[3] = v;

    d = dot_dop14_n2(v);
    dop[4] = min(dop[4], d);
    if (d == dop[4]) points[4] = v;
    dop[5] = max(dop[5], d);
    if (d == dop[5]) points[5] = v;

    d = dot_dop14_n3(v);
    dop[6] = min(dop[6], d);
    if (d == dop[6]) points[6] = v;
    dop[7] = max(dop[7], d);
    if (d == dop[7]) points[7] = v;

    d = dot_dop14_n4(v);
    dop[8] = min(dop[8], d);
    if (d == dop[8]) points[8] = v;
    dop[9] = max(dop[9], d);
    if (d == dop[9]) points[9] = v;

    d = dot_dop14_n5(v);
    dop[10] = min(dop[10], d);
    if (d == dop[10]) points[10] = v;
    dop[11] = max(dop[11], d);
    if (d == dop[11]) points[11] = v;

    d = dot_dop14_n6(v);
    dop[12] = min(dop[12], d);
    if (d == dop[12]) points[12] = v;
    dop[13] = max(dop[13], d);
    if (d == dop[13]) points[13] = v;
}

void bvFitWithPoints(inout float dop[14], inout vec3[14] points, in vec3[14] pointsToFit)
{
    for (int i = 0; i < 14; ++i)
        bvFitWithPoints(dop, pointsToFit[i], points);
}

float[14] dopInitWithVertId(in vec3 v, in uint vId, out uint[14] vertId)
{
    for (int i = 0; i < 14; ++i)
        vertId[i] = vId;

    float dop[14];
    dop[0] = dot_dop14_n0(v);
    dop[1] = dop[0];
    dop[2] = dot_dop14_n1(v);
    dop[3] = dop[2];
    dop[4] = dot_dop14_n2(v);
    dop[5] = dop[4];
    dop[6] = dot_dop14_n3(v);
    dop[7] = dop[6];
    dop[8] = dot_dop14_n4(v);
    dop[9] = dop[8];
    dop[10] = dot_dop14_n5(v);
    dop[11] = dop[10];
    dop[12] = dot_dop14_n6(v);
    dop[13] = dop[12];
    return dop;
}

void bvFitWithVertId(inout float dop[14], in vec3 v, in uint vId, inout uint[14] vertId)
{
    float d;
    d = dot_dop14_n0(v);
    dop[0] = min(dop[0], d);
    if (d == dop[0]) vertId[0] = vId;
    dop[1] = max(dop[1], d);
    if (d == dop[1]) vertId[1] = vId;

    d = dot_dop14_n1(v);
    dop[2] = min(dop[2], d);
    if (d == dop[2]) vertId[2] = vId;
    dop[3] = max(dop[3], d);
    if (d == dop[3]) vertId[3] = vId;

    d = dot_dop14_n2(v);
    dop[4] = min(dop[4], d);
    if (d == dop[4]) vertId[4] = vId;
    dop[5] = max(dop[5], d);
    if (d == dop[5]) vertId[5] = vId;

    d = dot_dop14_n3(v);
    dop[6] = min(dop[6], d);
    if (d == dop[6]) vertId[6] = vId;
    dop[7] = max(dop[7], d);
    if (d == dop[7]) vertId[7] = vId;

    d = dot_dop14_n4(v);
    dop[8] = min(dop[8], d);
    if (d == dop[8]) vertId[8] = vId;
    dop[9] = max(dop[9], d);
    if (d == dop[9]) vertId[9] = vId;

    d = dot_dop14_n5(v);
    dop[10] = min(dop[10], d);
    if (d == dop[10]) vertId[10] = vId;
    dop[11] = max(dop[11], d);
    if (d == dop[11]) vertId[11] = vId;

    d = dot_dop14_n6(v);
    dop[12] = min(dop[12], d);
    if (d == dop[12]) vertId[12] = vId;
    dop[13] = max(dop[13], d);
    if (d == dop[13]) vertId[13] = vId;
}

void bvFitWithVertId(inout float[14] dop, inout uint[14] vIds, in float[14] dopToFit, in uint[14] vIdsToFit)
{
    for (int i = 0; i < 7; i++) {
        int minId = 2 * i + 0;
        int maxId = 2 * i + 1;
        if (dopToFit[minId] < dop[minId]) {
            dop[minId] = dopToFit[minId];
            vIds[minId] = vIdsToFit[minId];
        }
        if (dopToFit[maxId] > dop[maxId]) {
            dop[maxId] = dopToFit[maxId];
            vIds[maxId] = vIdsToFit[maxId];
        }
    }
}

void bvFit(inout vec3[14] p0, in vec3[14] p1)
{
    float[14] dop = dopInit(p0[0]);
    bvFit(dop, p0[1]);
    bvFit(dop, p0[2]);
    bvFit(dop, p0[3]);
    bvFit(dop, p0[4]);
    bvFit(dop, p0[5]);
    bvFit(dop, p0[6]);
    bvFit(dop, p0[7]);
    bvFit(dop, p0[8]);
    bvFit(dop, p0[9]);
    bvFit(dop, p0[10]);
    bvFit(dop, p0[11]);
    bvFit(dop, p0[12]);
    bvFit(dop, p0[13]);

    bvFitWithPoints(dop, p1[0], p0);
    bvFitWithPoints(dop, p1[1], p0);
    bvFitWithPoints(dop, p1[2], p0);
    bvFitWithPoints(dop, p1[3], p0);
    bvFitWithPoints(dop, p1[4], p0);
    bvFitWithPoints(dop, p1[5], p0);
    bvFitWithPoints(dop, p1[6], p0);
    bvFitWithPoints(dop, p1[7], p0);
    bvFitWithPoints(dop, p1[8], p0);
    bvFitWithPoints(dop, p1[9], p0);
    bvFitWithPoints(dop, p1[10], p0);
    bvFitWithPoints(dop, p1[11], p0);
    bvFitWithPoints(dop, p1[12], p0);
    bvFitWithPoints(dop, p1[13], p0);
}

vec3 aabbCentroid(in float dop[14])
{
    return .5f * vec3((dop[0] + dop[1]), (dop[2] + dop[3]), (dop[4] + dop[5]));
}

float aabbSurfaceArea(in float dop[14])
{
    vec3 d = vec3((dop[1] - dop[0]), (dop[3] - dop[2]), (dop[5] - dop[4]));
    return 2.f * (d.x * d.y + d.x * d.z + d.z * d.y);
}

// surface area of a DOP14 by corner cutting
//   to improve numeric stability for certain scenes with small coordinates,
//   we scale the DOP14 by 1e3 and return the result scaled by 1e-6
float bvArea(in float[14] dop)
{
    for (int i = 0; i < 14; i++)
        dop[i] *= 1e3;

    vec3 diag = vec3((dop[1] - dop[0]), (dop[3] - dop[2]), (dop[5] - dop[4]));
    float result = 2.f * (diag.x * diag.y + diag.x * diag.z + diag.z * diag.y);

    // fast path for dummy dop (max area)
    if (dop[0] <= -1e30f && dop[1] >= 1e30f)
        return result;

    float d[8];
    d[0] = dop[6] - dot_dop14_n3(vec3(dop[0], dop[2], dop[4]));
    d[1] =          dot_dop14_n3(vec3(dop[1], dop[3], dop[5])) - dop[7];
    d[2] = dop[8] - dot_dop14_n4(vec3(dop[0], dop[2], dop[5]));
    d[3] =          dot_dop14_n4(vec3(dop[1], dop[3], dop[4])) - dop[9];
    d[4] = dop[10] - dot_dop14_n5(vec3(dop[0], dop[3], dop[4]));
    d[5] =           dot_dop14_n5(vec3(dop[1], dop[2], dop[5])) - dop[11];
    d[6] = dop[12] - dot_dop14_n6(vec3(dop[0], dop[3], dop[5]));
    d[7] =           dot_dop14_n6(vec3(dop[1], dop[2], dop[4])) - dop[13];

    // dop normals are not normalized, so we need to multiply by 1/sqrt(3)
    float accToSubtract = d[0]*d[0] + d[1]*d[1] + d[2]*d[2] + d[3]*d[3] + d[4]*d[4] + d[5]*d[5] + d[6]*d[6] + d[7]*d[7];
    accToSubtract *= .6339745962155614f;

    float s[12];
    // X pairs: 0:7, 1:6, 2:5, 3:4
    s[0] = max(0.f, d[0] + d[7] - diag.x);
    s[1] = max(0.f, d[1] + d[6] - diag.x);
    s[2] = max(0.f, d[2] + d[5] - diag.x);
    s[3] = max(0.f, d[3] + d[4] - diag.x);
    // Y pairs: 0:4, 1:5, 2:6, 3:7
    s[4] = max(0.f, d[0] + d[4] - diag.y);
    s[5] = max(0.f, d[1] + d[5] - diag.y);
    s[6] = max(0.f, d[2] + d[6] - diag.y);
    s[7] = max(0.f, d[3] + d[7] - diag.y);
    // Z pairs: 0:2, 1:3, 4:6, 5:7
    s[8]  = max(0.f, d[0] + d[2] - diag.z);
    s[9]  = max(0.f, d[1] + d[3] - diag.z);
    s[10] = max(0.f, d[4] + d[6] - diag.z);
    s[11] = max(0.f, d[5] + d[7] - diag.z);

    float accToAdd = s[0]*s[0] + s[1]*s[1] + s[2]*s[2] + s[3]*s[3]
                   + s[4]*s[4] + s[5]*s[5] + s[6]*s[6] + s[7]*s[7]
                   + s[8]*s[8] + s[9]*s[9] + s[10]*s[10] + s[11]*s[11];
    accToAdd *= .13397459621556135f;

    return (result - accToSubtract + accToAdd) * 1e-6;
}


#ifndef EPS
#define EPS 1e-5f
#endif
// reference implementation of face clipping vertex enumeration for DOP14
// bvArea_faceClipping() should work as a drop-in replacement for bvArea()
// it is significantly slower than bvArea()
void GetFace(in float dop[14], in int faceId, out vec3 direction, out float d)
{
    const int di = (faceId >> 1);
    if ((faceId & 1) != 0) {
        direction = DOP14_NORMAL[di];
        d = -dop[faceId];
    } else {
        direction = -DOP14_NORMAL[di];
        d = dop[faceId];
    }
}
int MajorAxis(in vec3 v)
{
    int axis = 0;
    float max = abs(v.x);
    if (abs(v.y) > max) {
        axis = 1;
        max = abs(v.y);
    }
    if (abs(v.z) > max) {
        axis = 2;
        max = abs(v.z);
    }
    return axis;
}
bool Solve3(in vec3 a1, in vec3 a2, in vec3 a3, in vec3 b, out vec3 result)
{
    // Calculate the determinant of the coefficient matrix
    float detA = a1.x * (a2.y * a3.z - a3.y * a2.z) - a1.y * (a2.x * a3.z - a3.x * a2.z) + a1.z * (a2.x * a3.y - a3.x * a2.y);
    // If the determinant is zero, then either there is no solution or there are infinitely many solutions
    if (abs(detA) < EPS)
        return false;
    // Calculate the determinants of the X, Y, and Z matrices
    float detX = b.x * (a2.y * a3.z - a2.z * a3.y) - a1.y * (b.y * a3.z - b.z * a2.z) + a1.z * (a3.y * b.y - b.z * a2.y);
    float detY = a1.x * (b.y * a3.z - a2.z * b.z) - b.x * (a2.x * a3.z - a3.x * a2.z) + a1.z * (a2.x * b.z - a3.x * b.y);
    float detZ = a1.x * (a2.y * b.z - a3.y * b.y) - a1.y * (a2.x * b.z - a3.x * b.y) + b.x * (a2.x * a3.y - a3.x * a2.y);
    // Calculate the solutions for X, Y, and Z
    result = vec3(detX / detA, detY / detA, detZ / detA);
    return true;
}

bool EdgeIntersection(in vec3 a, in vec3 b, in vec3 normal, in float d, out vec3 result)
{
    vec3 direction = b - a;
    float dp = dot(normal, direction);
    if (abs(dp) < EPS)
    return false;
    float t = (-d - dot(normal, a)) / dp;
    result = a + t * direction;
    return true;
}

// surface area of a DOP14 by face clipping
float bvArea_faceClipping(in float[14] dop)
{
    float result = 0.f;

    // fast path for dummy dop (max area)
    if (dop[0] == -1e30f && dop[1] == 1e30f)
        return 1e30f * 1e30f * 6;

    vec3 v[2][12];
    int8_t side[12];
    int nvin, nvout;

    int In = 0;
    int Out = 1;

    for (int i = 0; i < 14; ++i) {
        vec3 ni;
        float di;
        GetFace(dop, i, ni, di);

        if (i < 6) {
            switch (i) {
            case 0:
                v[In][3] = vec3(dop[0], dop[2], dop[4]);
                v[In][2] = vec3(dop[0], dop[3], dop[4]);
                v[In][1] = vec3(dop[0], dop[3], dop[5]);
                v[In][0] = vec3(dop[0], dop[2], dop[5]);
                break;
            case 1:
                v[In][0] = vec3(dop[1], dop[2], dop[4]);
                v[In][1] = vec3(dop[1], dop[3], dop[4]);
                v[In][2] = vec3(dop[1], dop[3], dop[5]);
                v[In][3] = vec3(dop[1], dop[2], dop[5]);
                break;
            case 2:
                v[In][0] = vec3(dop[0], dop[2], dop[4]);
                v[In][1] = vec3(dop[1], dop[2], dop[4]);
                v[In][2] = vec3(dop[1], dop[2], dop[5]);
                v[In][3] = vec3(dop[0], dop[2], dop[5]);
                break;
            case 3:
                v[In][3] = vec3(dop[0], dop[3], dop[4]);
                v[In][2] = vec3(dop[1], dop[3], dop[4]);
                v[In][1] = vec3(dop[1], dop[3], dop[5]);
                v[In][0] = vec3(dop[0], dop[3], dop[5]);
                break;
            case 4:
                v[In][3] = vec3(dop[0], dop[2], dop[4]);
                v[In][2] = vec3(dop[1], dop[2], dop[4]);
                v[In][1] = vec3(dop[1], dop[3], dop[4]);
                v[In][0] = vec3(dop[0], dop[3], dop[4]);
                break;
            case 5:
                v[In][0] = vec3(dop[0], dop[2], dop[5]);
                v[In][1] = vec3(dop[1], dop[2], dop[5]);
                v[In][2] = vec3(dop[1], dop[3], dop[5]);
                v[In][3] = vec3(dop[0], dop[3], dop[5]);
                break;
            }
        }
        // construct a quad by clipping that face by four planes of AABB
        else {
            int axis = MajorAxis(ni);

            switch (axis) {
                case 0:
                Solve3(ni, vec3(0, 1, 0), vec3(0, 0, 1), vec3(-di, dop[2], dop[4]), v[In][0]);
                Solve3(ni, vec3(0, 1, 0), vec3(0, 0, 1), vec3(-di, dop[3], dop[4]), v[In][1]);
                Solve3(ni, vec3(0, 1, 0), vec3(0, 0, 1), vec3(-di, dop[3], dop[5]), v[In][2]);
                Solve3(ni, vec3(0, 1, 0), vec3(0, 0, 1), vec3(-di, dop[2], dop[5]), v[In][3]);
                break;
                case 1:
                Solve3(ni, vec3(1, 0, 0), vec3(0, 0, 1), vec3(-di, dop[0], dop[4]), v[In][3]);
                Solve3(ni, vec3(1, 0, 0), vec3(0, 0, 1), vec3(-di, dop[1], dop[4]), v[In][2]);
                Solve3(ni, vec3(1, 0, 0), vec3(0, 0, 1), vec3(-di, dop[1], dop[5]), v[In][1]);
                Solve3(ni, vec3(1, 0, 0), vec3(0, 0, 1), vec3(-di, dop[0], dop[5]), v[In][0]);
                break;
                case 2:
                Solve3(ni, vec3(1, 0, 0), vec3(0, 1, 0), vec3(-di, dop[0], dop[2]), v[In][0]);
                Solve3(ni, vec3(1, 0, 0), vec3(0, 1, 0), vec3(-di, dop[1], dop[2]), v[In][1]);
                Solve3(ni, vec3(1, 0, 0), vec3(0, 1, 0), vec3(-di, dop[1], dop[3]), v[In][2]);
                Solve3(ni, vec3(1, 0, 0), vec3(0, 1, 0), vec3(-di, dop[0], dop[3]), v[In][3]);
                break;
            }

            if (ni[axis] < 0) {
                vec3 tmp = v[In][0];
                v[In][0] = v[In][3];
                v[In][3] = tmp;
                tmp = v[In][1];
                v[In][1] = v[In][2];
                v[In][2] = tmp;
            }
        }

        nvin = 4;
        for (int j = 0; j < 14; ++j) {
            // skip the identical plane and the plane with the same normal
            // TODO: gpu friendly implementation
            if (i / 2 == j / 2)
                continue;
            vec3 nj;
            float dj;
            GetFace(dop, j, nj, dj);

            int back = 0;
            int front = 0;

            for (int k = 0; k < nvin; ++k) {
                float d = dot(nj, v[In][k]) + dj;
                if (d > EPS) {
                    side[k] = int8_t(1);
                    ++front;
                }
                else if (d < -EPS) {
                    side[k] = int8_t(-1);
                    ++back;
                }
                else {
                    side[k] = int8_t(0);
                }
            }
            // this face has been clipped completely - > break the loop
            if (back == 0) {
                nvin = 0;
                break;
            }
            // will have to clip
            if (front != 0) {
                nvout = 0;
                for (int k = 0; k < nvin; ++k) {
                    int kk = (k + 1) % nvin;
                    // check the edge k -> k+1
                    if (side[k] <= 0) {
                        v[Out][nvout++] = v[In][k];
                        // edge going from correct to wrong -> insert k and clipped vertex
                        if (side[k] < 0 && side[kk] > 0) {
                            if (EdgeIntersection(v[In][k], v[In][kk], nj, dj, v[Out][nvout]))
                                nvout++;
                        }
                    }
                    else {
                        // edge going from wrong to correct -> insert clipped vertex
                        if (side[kk] < 0) {
                            if (EdgeIntersection(v[In][k], v[In][kk], nj, dj, v[Out][nvout]))
                                nvout++;
                        }
                    }
                    // otherwise the whole edge is on the wrong side -> safely clip vertex k
                }
                int tmp = In;
                In = Out;
                Out = tmp;
                nvin = nvout;
            }
        }

        // calculate the surface area of the clipped face
        if (nvin > 2) {
            vec3 sumV = cross(v[In][nvin - 1], v[In][0]);
            for (int k = 0; k < nvin - 1; ++k)
                sumV += cross(v[In][k], v[In][k + 1]);
            result += .5f * abs(dot(ni, sumV));
        }
    }

    return result;
}

#endif