#pragma once

#include "../../../Config.h"
#include "../../../Stats.h"
#include "../../VCtx.h"
#include "Types.h"
#include <vLime/Compute.h>
#include <vLime/Memory.h>
#include <vLime/Timestamp.h>
#include <vLime/vLime.h>

namespace backend::vulkan::bvh {

struct Collapsing {
    explicit Collapsing(VCtx ctx);

    [[nodiscard]] Bvh GetBVH() const;
    [[nodiscard]] bool NeedsRecompute(config::Collapsing const& buildConfig)
    {
        auto const cfgChanged { config != buildConfig };
        config = buildConfig;
        return cfgChanged && config.bv != config::BV::eNone;
    }

    void Compute(vk::CommandBuffer commandBuffer, Bvh const& inputBvh, vk::DeviceAddress geometryDescriptor);
    void ReadRuntimeData();
    [[nodiscard]] stats::Collapsing GatherStats(BvhStats const& bvhStats);

private:
    VCtx ctx;
    config::Collapsing config;

    lime::PipelineCompute pCollapse;

    struct Metadata {
        u32 nodeCountLeaf { 0 };
        u32 nodeCountTotal { 0 };

        u32 workgroupSize { 0 };
    } metadata;

    enum class Buffer {
        eBVH,
        eBVHTriangles,
        eBVHTriangleIDs,

        eScheduler,

        eTraversalCounters,
        eNodeState,
        eSAHCost,
        eLeafNodes,
        eNewNodeId,
        eNewTriId,

        eCollapsedNodeCounts,

        eStats,
        eDebug,
    };

    lime::Buffer stagingBuffer;
    std::unordered_map<Buffer, lime::Buffer> buffersOut;
    std::unordered_map<Buffer, lime::Buffer> buffersIntermediate;

    void reloadPipelines();
    void alloc();
    void freeIntermediate();
    void freeAll();

    void collapse(vk::CommandBuffer commandBuffer, Bvh const& inputBvh, vk::DeviceAddress geometryDescriptor);

    lime::SingleTimer timestamps;
};

}
