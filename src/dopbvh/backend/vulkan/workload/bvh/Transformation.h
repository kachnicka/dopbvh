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

struct Transformation {
    explicit Transformation(VCtx ctx);

    [[nodiscard]] Bvh GetBVH() const;
    [[nodiscard]] bool NeedsRecompute(config::Transformation const& buildConfig)
    {
        auto const cfgChanged { config != buildConfig };
        config = buildConfig;
        return cfgChanged && config.bv != config::BV::eNone;
    }

    void Compute(vk::CommandBuffer commandBuffer, Bvh const& inputBvh, vk::DeviceAddress geometryDescriptor);
    void ReadRuntimeData();
    [[nodiscard]] stats::Transformation GatherStats(BvhStats const& bvhStats);

private:
    VCtx ctx;
    config::Transformation config;

    lime::PipelineCompute pTransform;

    struct Metadata {
        u32 nodeCountLeaf { 0 };
        u32 nodeCountTotal { 0 };
        vk::DeviceAddress bvhTriangles { 0 };
        vk::DeviceAddress bvhTriangleIDs { 0 };

        u32 workgroupSize { 0 };
    } metadata;

    enum class Buffer {
        eBVH,
        eTraversalCounters,

        eDitoPoints,
        eObb,
        eScheduler,

        eTimes_TMP,
    };

    std::unordered_map<Buffer, lime::Buffer> buffersOut;
    std::unordered_map<Buffer, lime::Buffer> buffersIntermediate;

    void reloadPipelines(config::BV srcBv);
    void alloc();
    void freeIntermediate();
    void freeAll();

    void transform_dop14(vk::CommandBuffer commandBuffer, Bvh const& inputBvh, vk::DeviceAddress geometryDescriptor);
    void transform_obb(vk::CommandBuffer commandBuffer, Bvh const& inputBvh, vk::DeviceAddress geometryDescriptor);

    lime::SingleTimer timestamps;
};

}
