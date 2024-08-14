#ifndef PLOCPP_DATA_H
#define PLOCPP_DATA_H

#ifndef INCLUDE_FROM_SHADER
#include <cstdint>
using vec3 = float[3];
namespace data_plocpp {
#else
#include "bv_aabb.glsl"
#define INVALID_ID -1
#endif

struct Morton32KeyVal {
    uint32_t key;
    uint32_t mortonCode;
};

struct DLPartitionData {
    uint32_t aggregate;
    uint32_t prefix;
};

struct PLOCData {
    uint32_t bvOffset;
    uint32_t iterationClusterCount;
    uint32_t iterationTaskCount;
    uint32_t fill;
    uint32_t iterationCounter;
};

struct PC_MortonGlobal {
    vec3 sceneAabbCubedMin;
    float sceneAabbNormalizationScale;
    uint64_t mortonAddress;
    uint64_t bvhAddress;
    uint64_t bvhTrianglesAddress;
    uint64_t bvhTriangleIndicesAddress;
    uint64_t auxBufferAddress;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 56 };
#endif
};

struct PC_MortonPerGeometry {
    uint64_t idxAddress;
    uint64_t vtxAddress;
    uint32_t globalTriangleIdBase;
    uint32_t sceneNodeId;
    uint32_t triangleCount;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 28 };
#endif
};

struct PC_CopySortedNodeIds {
    uint64_t mortonAddress;
    uint64_t nodeIdAddress;
    uint32_t clusterCount;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 20 };
#endif
};

struct PC_PlocppIteration {
    uint64_t bvhAddress;
    uint64_t nodeId0Address;
    uint64_t nodeId1Address;
    uint64_t dlWorkBufAddress;
    uint64_t runtimeDataAddress;
    uint64_t auxBufferAddress;

    uint32_t clusterCount;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 52 };
#endif
};

struct PC_TransformToDOP {
    uint64_t bvhAddress;
    uint64_t bvhTriangleIndicesAddress;
    uint64_t bvhDOPAddress;

    uint64_t bvhNodeCountsAddress;
    uint64_t geometryDescriptorAddress;
    uint64_t countersAddress;

    // uint64_t debugBufferAddress;

    uint32_t leafNodeCount;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 52 };
#endif
};

struct PC_TransformToOBB {
    uint64_t bvhInAddress;
    uint64_t bvhInTriangleIndicesAddress;
    uint64_t bvhOutAddress;

    uint64_t ditoPointsAddress;
    uint64_t obbAddress;

    uint64_t geometryDescriptorAddress;
    uint64_t traversalCounterAddress;
    uint64_t schedulerDataAddress;

    uint64_t timesAddress_TMP;

    uint32_t leafNodeCount;

#ifndef INCLUDE_FROM_SHADER
    static constexpr uint32_t SCALAR_SIZE { 68 + 8 };
#endif
};


#define SC_PLOCPP_ITERATION \
    layout(local_size_x_id = 0) in; \
    layout(constant_id = 1) const uint32_t SC_subgroupSize = 32u; \
    layout(constant_id = 2) const uint32_t SC_plocRadius = 16u;

struct SC {
    uint32_t sizeWorkgroup;
    uint32_t sizeSubgroup;
    uint32_t plocRadius;

#ifndef INCLUDE_FROM_SHADER
    [[nodiscard]] uint32_t getChunkSize() const
    {
        return sizeWorkgroup - 4 * plocRadius;
    }
#endif
};

struct DiTO14ProjectedPoints
{
    vec3 p[14];
};

#ifndef INCLUDE_FROM_SHADER
}
#else
#include "data_types_general.glsl"
layout (buffer_reference, scalar) buffer Morton32KeyVals { Morton32KeyVal keyval[]; };
layout (buffer_reference, scalar) buffer DLWorkBuf { DLPartitionData val[]; };
layout (buffer_reference, scalar) buffer DiTO14Points { DiTO14ProjectedPoints val[]; };

#endif


#endif // PLOCPP_DATA_H