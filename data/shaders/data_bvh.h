#ifndef BVH_DATA_H
#define BVH_DATA_H

#ifndef INCLUDE_FROM_SHADER
#include <cstdint>
using uvec3 = uint32_t[3];
using vec3 = float[3];
using vec4 = float[4];
namespace data_bvh {
#else
#include "bv_aabb.glsl"
#endif

#define RAY_TYPE_PRIMARY 0
#define RAY_TYPE_SHADOW 1
#define RAY_TYPE_SECONDARY 2

struct Ray {
    vec4 o;
    vec4 d;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 32 };
#endif
};

struct RayBufferMetadata {
    uint32_t rayCount;
    uint32_t rayTracedCount;
    uint32_t depth_TMP;
    uint32_t accumulatedSampleCount;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 16 };
#endif
};

struct RayTraceResult {
    uint32_t instanceId;
    uint32_t primitiveId;
    uint32_t bvIntersectionCount;
    float t;
    float u;
    float v;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 24 };
#endif
};

struct RayPayload {
    vec3 throughput;
    uint32_t packedPosition;
    uint32_t seed;

    uint32_t padding[3];

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 32 };
#endif
};

struct PC_PT {
    vec4 dirLight;
    uint64_t schedulerDataAddress;
    uint64_t ptStatsAddress;

    uint64_t rayBufferMetadataAddress;
    uint64_t rayBuffer0Address;
    uint64_t rayBuffer1Address;
    uint64_t rayPayload0Address;
    uint64_t rayPayload1Address;
    uint64_t rayTraceResultAddress;

    uint64_t geometryDescriptorAddress;
    uint64_t bvhAddress;
    uint64_t bvhTrianglesAddress;
    uint64_t bvhTriangleIndicesAddress;

    uint64_t auxBufferAddress;

    uint32_t samplesComputed;
    uint32_t samplesToComputeThisFrame;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 112 + 16 };
#endif
};

struct RayTimes {
    uint64_t timer;
    uint32_t rayCount;
    uint32_t padding;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 16 };
#endif
};

struct PT_Stats {
    RayTimes times[8];
    uint64_t timerStart;
    uint64_t timerEnd;
    uint32_t traversedNodes;
    uint32_t testedTriangles;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 24 + 8 * RayTimes::SCALAR_SIZE };
#endif
};

struct PC_SAHCost {
    uint64_t bvhAddress;
    uint64_t resultBufferAddress;
    uint32_t nodeCount;
    float c_t;
    float c_i;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 28 };
#endif
};

struct PC_BvhStats {
    uint64_t bvhAddress;
    uint64_t bvhAuxAddress;
    uint64_t resultBufferAddress;
    uint32_t nodeCount;
    float c_t;
    float c_i;
    float sceneAabbSurfaceArea;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 40 };
#endif
};

struct NodeBvhBinary {
#ifndef INCLUDE_FROM_SHADER
    float bv[6];
#else
    Aabb bv;
#endif
    int32_t size;
    int32_t parent;
    int32_t c0;
    int32_t c1;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 40 };
#endif
};

struct NodeBvhBinaryCompressed {
#ifndef INCLUDE_FROM_SHADER
    float bv[12];
#else
    float bv[12];
    // Aabb aabb0;
    // Aabb aabb1;
#endif
    int32_t size;
    int32_t parent;
    int32_t c0;
    int32_t c1;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 64 };
#endif
};

struct NodeBvhBinaryDOP14 {
#ifndef INCLUDE_FROM_SHADER
    float bv[14];
#else
    float bv[14];
#endif
    int32_t size;
    int32_t parent;
    int32_t c0;
    int32_t c1;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 72 };
#endif
};

struct NodeBvhBinaryDOP14Compressed {
#ifndef INCLUDE_FROM_SHADER
    float bv[28];
#else
    float bv[28];
#endif
    int32_t size;
    int32_t parent;
    int32_t c0;
    int32_t c1;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 128 };
#endif
};

struct NodeBvhBinaryDiTO14Points {
#ifndef INCLUDE_FROM_SHADER
    float bv[42];
#else
    vec3[14] bv;
#endif
    int32_t size;
    int32_t parent;
    int32_t c0;
    int32_t c1;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 184 };
#endif
};

struct NodeBvhBinaryOBB {
#ifndef INCLUDE_FROM_SHADER
    float bv[12];
#else
    mat4x3 bv;
#endif
    int32_t size;
    int32_t parent;
    int32_t c0;
    int32_t c1;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 64 };
#endif
};

struct NodeBvhBinaryOBBCompressed {
#ifndef INCLUDE_FROM_SHADER
    float bv[24];
#else
    mat4x3[2] bv;
#endif
    int32_t size;
    int32_t parent;
    int32_t c0;
    int32_t c1;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 112 };
#endif
};


struct BvhTriangleIndex {
    uint32_t nodeId;
    uint32_t triangleId;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 8 };
#endif
};

struct BvhTriangle {
    vec4 v0;
    vec4 v1;
    vec4 v2;
};

struct NodeBvhBinaryDOP14Compressed_SPLIT {
    float bv[16];

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 64 };
#endif
};

#ifndef INCLUDE_FROM_SHADER
}
#else
layout (buffer_reference, scalar) buffer BvhBinary { NodeBvhBinary node[]; };
layout (buffer_reference, scalar) buffer BvhBinaryCompressed { NodeBvhBinaryCompressed node[]; };
layout (buffer_reference, scalar) buffer BvhBinaryDOP14 { NodeBvhBinaryDOP14 node[]; };
layout (buffer_reference, scalar) buffer BvhBinaryDOP14Compressed { NodeBvhBinaryDOP14Compressed node[]; };
layout (buffer_reference, scalar) buffer BvhBinaryDOP14Compressed_SPLIT { NodeBvhBinaryDOP14Compressed_SPLIT node[]; };
layout (buffer_reference, scalar) buffer BvhBinaryDiTO14Points { NodeBvhBinaryDiTO14Points node[]; };
layout (buffer_reference, scalar) buffer BvhBinaryOBB { NodeBvhBinaryOBB node[]; };
layout (buffer_reference, scalar) buffer BvhBinaryOBBCompressed { NodeBvhBinaryOBBCompressed node[]; };
layout (buffer_reference, scalar) buffer BvhCost { float t; float i; };
layout (buffer_reference, scalar) buffer BvhStats { float sat; float sai; float ct; float ci; uint leafSizeSum; uint leafSizeMin; uint leafSizeMax; };
layout (buffer_reference, scalar) buffer BvhTriangles { BvhTriangle t[]; };
layout (buffer_reference, scalar) buffer BvhTriangleIndices { BvhTriangleIndex val[]; };
layout (buffer_reference, scalar) buffer RayBuf { Ray ray[]; };
layout (buffer_reference, scalar) buffer RayBufferMetadata_ref { RayBufferMetadata data_0; RayBufferMetadata data_1; };
layout (buffer_reference, scalar) buffer RayTraceResultBuf { RayTraceResult result[]; };
layout (buffer_reference, scalar) buffer RayPayloadBuf { RayPayload val[]; };
layout (buffer_reference, scalar) buffer Stats { PT_Stats data; };
#endif


#endif // BVH_DATA_H
