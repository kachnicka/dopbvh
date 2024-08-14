#pragma once

#include "../../../Config.h"
#include "../../../Stats.h"
#include "../../VCtx.h"
#include "Types.h"
#include <vLime/Compute.h>
#include <vLime/Timestamp.h>
#include <vLime/vLime.h>

namespace backend::vulkan::bvh {

struct Compression {
    explicit Compression(VCtx ctx);

    [[nodiscard]] Bvh GetBVH() const;
    [[nodiscard]] bool NeedsRecompute(config::Compression const& buildConfig)
    {
        auto const cfgChanged { config != buildConfig };
        config = buildConfig;
        return cfgChanged && config.bv != config::BV::eNone;
    }

    void Compute(vk::CommandBuffer commandBuffer, Bvh const& inputBvh);
    void ReadRuntimeData();
    [[nodiscard]] stats::Compression GatherStats(BvhStats const& bvhStats);

private:
    config::Compression config;
    VCtx ctx;

    lime::PipelineCompute pCompress;

    struct Metadata {
        u32 nodeCountLeaf { 0 };
        u32 nodeCountTotal { 0 };
        u32 inputNodeCountTotal { 0 };
        vk::DeviceAddress bvhTriangles { 0 };
        vk::DeviceAddress bvhTriangleIDs { 0 };

        u32 workgroupSize { 0 };
    } metadata;

    enum class Buffer {
        eBVH,

        eScheduler,
        eRuntimeData,
    };

    std::unordered_map<Buffer, lime::Buffer> buffersOut;
    std::unordered_map<Buffer, lime::Buffer> buffersIntermediate;
    lime::Buffer bBvhAux;

    void reloadPipelines();
    void alloc();
    void freeIntermediate();
    void freeAll();

    void compress(vk::CommandBuffer commandBuffer, Bvh const& inputBvh);

    lime::SingleTimer timestamps;
};

}
