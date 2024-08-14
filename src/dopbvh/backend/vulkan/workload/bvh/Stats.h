#pragma once

#include "../../../Config.h"
#include "../../VCtx.h"
#include "Types.h"
#include <vLime/Compute.h>
#include <vLime/vLime.h>

namespace backend::vulkan::bvh {

struct Stats {
    config::Stats config;

    BvhStats* data { nullptr };

    explicit Stats(VCtx ctx);

    void Compute(vk::CommandBuffer commandBuffer, config::Stats const& buildCfg, Bvh const& bvh);

    void SetSceneAabbSurfaceArea(f32 sa) { metadata.sceneAabbSurfaceArea = sa; }

private:
    VCtx ctx;

    lime::PipelineCompute pStats;
    lime::PipelineCompute pStatsCompressed;

    struct Metadata {
        f32 sceneAabbSurfaceArea { std::numeric_limits<f32>::infinity() };
    } metadata;

    lime::Buffer bStats;

    u32 workgroupSize { 0 };

    void reloadPipelines(Bvh::Layout layout);
    void alloc();
    void freeAll();

    void stats(vk::CommandBuffer commandBuffer, Bvh const& bvh);
};

}
