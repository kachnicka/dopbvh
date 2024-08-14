#include "Stats.h"

#include "data_bvh.h"

namespace backend::vulkan::bvh {

static u32 CreateSpecializationConstants(vk::PhysicalDevice pd)
{
    auto const prop2 { pd.getProperties2() };
    return prop2.properties.limits.maxComputeWorkGroupSize[0];
}

Stats::Stats(VCtx ctx)
    : ctx(ctx)
{
    alloc();
}

void Stats::Compute(vk::CommandBuffer commandBuffer, config::Stats const& buildCfg, Bvh const& bvh)
{
    config = buildCfg;
    reloadPipelines(bvh.layout);
    stats(commandBuffer, bvh);
}

void Stats::reloadPipelines(Bvh::Layout layout)
{
    workgroupSize = CreateSpecializationConstants(ctx.pd);

    static std::array<vk::SpecializationMapEntry, 1> constexpr entries {
        vk::SpecializationMapEntry { 0, 0, sizeof(uint32_t) },
    };
    vk::SpecializationInfo sInfo { 1, entries.data(), 4, &workgroupSize };

    switch (config.bv) {
    case config::BV::eAABB:
        pStats = { ctx.d, ctx.sCache, "gen_stats_aabb.comp.spv", sInfo };
        pStatsCompressed = { ctx.d, ctx.sCache, "gen_stats_aabb_c.comp.spv", sInfo };
        break;
    case config::BV::eDOP14:
        pStats = { ctx.d, ctx.sCache, "gen_stats_dop14.comp.spv", sInfo };
        if (layout == Bvh::Layout::eBinaryCompressed_dop14Split)
            pStatsCompressed = { ctx.d, ctx.sCache, "gen_stats_dop14_split_c.comp.spv", sInfo };
        else
            pStatsCompressed = { ctx.d, ctx.sCache, "gen_stats_dop14_c.comp.spv", sInfo };
        break;
    case config::BV::eOBB:
        pStats = { ctx.d, ctx.sCache, "gen_stats_obb.comp.spv", sInfo };
        pStatsCompressed = { ctx.d, ctx.sCache, "gen_stats_obb_c.comp.spv", sInfo };
        break;
    default:
        pStats = {};
        pStatsCompressed = {};
    }
}

void Stats::alloc()
{
    freeAll();
    ctx.memory.cleanUp();

    using bfub = vk::BufferUsageFlagBits;
    lime::AllocRequirements aReq {
        .memoryUsage = lime::DeviceMemoryUsage::eDeviceToHost,
    };
    vk::BufferCreateInfo cInfo {
        .size = sizeof(BvhStats),
        .usage = bfub::eStorageBuffer | bfub::eShaderDeviceAddress | bfub::eTransferSrc | bfub::eTransferDst,
    };

    bStats = ctx.memory.alloc(aReq, cInfo, "bvh_stats");
    data = static_cast<BvhStats*>(bStats.getMapping());
}

void Stats::freeAll()
{
    bStats.reset();
}

void Stats::stats(vk::CommandBuffer commandBuffer, Bvh const& bvh)
{
    *data = BvhStats {};

    data_bvh::PC_BvhStats pcSahCost {
        .bvhAddress = bvh.bvh,
        .bvhAuxAddress = bvh.bvhAux,
        .resultBufferAddress = bStats.getDeviceAddress(ctx.d),
        .nodeCount = bvh.nodeCountTotal,
        .c_t = config.c_t,
        .c_i = config.c_i,
        .sceneAabbSurfaceArea = metadata.sceneAabbSurfaceArea,
    };
    if (bvh.layout == Bvh::Layout::eBinaryStandard)
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pStats.get());
    else
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pStatsCompressed.get());

    commandBuffer.pushConstants(pStats.layout.pipeline.get(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_BvhStats::SCALAR_SIZE, &pcSahCost);
    commandBuffer.dispatch(lime::divCeil(pcSahCost.nodeCount, workgroupSize), 1, 1);
}

}
