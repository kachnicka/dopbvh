#include "Transformation.h"

#include "data_bvh.h"
#include "data_plocpp.h"

namespace backend::vulkan::bvh {

static u32 CreateSpecializationConstants(vk::PhysicalDevice pd)
{
    auto const prop2 { pd.getProperties2() };
    return std::min(512u, prop2.properties.limits.maxComputeWorkGroupSize[0]);
}

Transformation::Transformation(VCtx ctx)
    : ctx(ctx)
    , timestamps(ctx.d, ctx.pd)
{
}

Bvh Transformation::GetBVH() const
{
    return {
        .bvh = buffersOut.at(Buffer::eBVH).getDeviceAddress(ctx.d),
        .triangles = metadata.bvhTriangles,
        .triangleIDs = metadata.bvhTriangleIDs,
        .nodeCountLeaf = metadata.nodeCountLeaf,
        .nodeCountTotal = metadata.nodeCountTotal,
        .bv = config.bv,
    };
}

void Transformation::Compute(vk::CommandBuffer commandBuffer, Bvh const& inputBvh, vk::DeviceAddress geometryDescriptor)
{
    metadata.nodeCountLeaf = inputBvh.nodeCountLeaf;
    metadata.nodeCountTotal = inputBvh.nodeCountTotal;
    metadata.bvhTriangles = inputBvh.triangles;
    metadata.bvhTriangleIDs = inputBvh.triangleIDs;

    reloadPipelines(inputBvh.bv);
    alloc();
    auto const fillZeros { [](vk::CommandBuffer commandBuffer, auto& buf) {
        commandBuffer.fillBuffer(buf.get(), 0, buf.getSizeInBytes(), 0);
    } };
    if (config.bv == config::BV::eOBB)
        fillZeros(commandBuffer, buffersIntermediate[Buffer::eScheduler]);
    fillZeros(commandBuffer, buffersIntermediate[Buffer::eTraversalCounters]);

    timestamps.Reset(commandBuffer);
    vk::MemoryBarrier bufferWriteBarrier { .srcAccessMask = vk::AccessFlagBits::eTransferWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), bufferWriteBarrier, nullptr, nullptr);
    timestamps.Begin(commandBuffer);
    if (config.bv == config::BV::eDOP14)
        transform_dop14(commandBuffer, inputBvh, geometryDescriptor);
    else if (config.bv == config::BV::eOBB)
        transform_obb(commandBuffer, inputBvh, geometryDescriptor);
    timestamps.End(commandBuffer);
}

void Transformation::ReadRuntimeData()
{
    if (config.bv == config::BV::eOBB) {
        struct Times {
            u64 project;
            u64 select;
            u64 refit;
            u64 finalise;
        } times = *static_cast<Times*>(buffersIntermediate[Buffer::eTimes_TMP].getMapping());

        static auto const timestampToMs = [pd = ctx.pd](u64 stamp) -> f32 {
            static auto const timestampValidBits { pd.getQueueFamilyProperties()[0].timestampValidBits };
            static auto const timestampPeriod { pd.getProperties().limits.timestampPeriod };

            if (timestampValidBits < sizeof(u64) * 8)
                stamp = stamp & ~(static_cast<u64>(-1) << timestampValidBits);
            return static_cast<f32>(stamp) * timestampPeriod * 1e-6f;
        };

        static_cast<void>(times);
        static_cast<void>(timestampToMs);
        // berry::Log::debug("OBB DiTO transformation:");
        // berry::Log::debug("Project: {:.2f} ms, Select: {:.2f} ms, Refit: {:.2f} ms, Finalise: {:.2f} ms",
        //     timestampToMs(times.project), timestampToMs(times.select), timestampToMs(times.refit), timestampToMs(times.finalise));
    }
}

stats::Transformation Transformation::GatherStats(BvhStats const& bvhStats)
{
    stats::Transformation stats;
    stats.timeTotal = timestamps.ReadTimeNs() * 1e-6f;

    stats.saIntersect = bvhStats.saIntersect;
    stats.saTraverse = bvhStats.saTraverse;
    stats.costTotal = bvhStats.costIntersect + bvhStats.costTraverse;

    stats.nodeCountTotal = metadata.nodeCountTotal;
    stats.leafSizeMin = bvhStats.leafSizeMin;
    stats.leafSizeMax = bvhStats.leafSizeMax;
    stats.leafSizeAvg = static_cast<f32>(bvhStats.leafSizeSum) / static_cast<f32>(metadata.nodeCountLeaf);

    return stats;
}

void Transformation::reloadPipelines(config::BV srcBv)
{
    metadata.workgroupSize = CreateSpecializationConstants(ctx.pd);

    static std::array<vk::SpecializationMapEntry, 1> constexpr entries {
        vk::SpecializationMapEntry { 0, 0, sizeof(u32) },
    };
    vk::SpecializationInfo sInfo { 1, entries.data(), 4, &metadata.workgroupSize };

    switch (srcBv) {
    case config::BV::eAABB:
        switch (config.bv) {
        case config::BV::eDOP14:
            pTransform = { ctx.d, ctx.sCache, "gen_transform_aabb_dop14.comp.spv", sInfo };
            break;
        case config::BV::eOBB:
            pTransform = { ctx.d, ctx.sCache, "gen_transform_aabb_obb.comp.spv", sInfo };
            break;
        default:
            pTransform = {};
        }
        break;
    case config::BV::eDOP14:
        pTransform = { ctx.d, ctx.sCache, "gen_transform_dop14_obb.comp.spv", sInfo };
        break;
    default:
        pTransform = {};
    }
}

void Transformation::transform_dop14(vk::CommandBuffer commandBuffer, Bvh const& inputBvh, vk::DeviceAddress geometryDescriptor)
{
    vk::MemoryBarrier memoryBarrierCompute { .srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), memoryBarrierCompute, nullptr, nullptr);

    data_plocpp::PC_TransformToDOP pc {
        .bvhAddress = inputBvh.bvh,
        .bvhTriangleIndicesAddress = inputBvh.triangleIDs,
        .bvhDOPAddress = buffersOut[Buffer::eBVH].getDeviceAddress(ctx.d),

        // .bvhNodeCountsAddress = buffers[BufName::eCollapseNodeCounts].getDeviceAddress(ctx.d),
        .bvhNodeCountsAddress = 0,

        .geometryDescriptorAddress = geometryDescriptor,
        .countersAddress = buffersIntermediate[Buffer::eTraversalCounters].getDeviceAddress(ctx.d),

        .leafNodeCount = inputBvh.nodeCountLeaf,
    };

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pTransform.get());
    commandBuffer.pushConstants(pTransform.layout.pipeline.get(), vk::ShaderStageFlagBits::eCompute, 0, data_plocpp::PC_TransformToDOP::SCALAR_SIZE, &pc);
    commandBuffer.dispatch(lime::divCeil(inputBvh.nodeCountLeaf, metadata.workgroupSize), 1, 1);
}

void Transformation::transform_obb(vk::CommandBuffer commandBuffer, Bvh const& inputBvh, vk::DeviceAddress geometryDescriptor)
{
    vk::MemoryBarrier memoryBarrierCompute { .srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), memoryBarrierCompute, nullptr, nullptr);

    data_plocpp::PC_TransformToOBB pc {
        .bvhInAddress = inputBvh.bvh,
        .bvhInTriangleIndicesAddress = inputBvh.triangleIDs,
        .bvhOutAddress = buffersOut[Buffer::eBVH].getDeviceAddress(ctx.d),

        .ditoPointsAddress = buffersIntermediate[Buffer::eDitoPoints].getDeviceAddress(ctx.d),
        .obbAddress = buffersIntermediate[Buffer::eObb].getDeviceAddress(ctx.d),

        .geometryDescriptorAddress = geometryDescriptor,
        .traversalCounterAddress = buffersIntermediate[Buffer::eTraversalCounters].getDeviceAddress(ctx.d),
        .schedulerDataAddress = buffersIntermediate[Buffer::eScheduler].getDeviceAddress(ctx.d),

        .timesAddress_TMP = buffersIntermediate[Buffer::eTimes_TMP].getDeviceAddress(ctx.d),

        .leafNodeCount = inputBvh.nodeCountLeaf,
    };

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pTransform.get());
    commandBuffer.pushConstants(pTransform.layout.pipeline.get(), vk::ShaderStageFlagBits::eCompute, 0, data_plocpp::PC_TransformToOBB::SCALAR_SIZE, &pc);
    commandBuffer.dispatch(2048, 1, 1);
}

void Transformation::freeIntermediate()
{
    buffersIntermediate.clear();
}

void Transformation::freeAll()
{
    freeIntermediate();
    buffersOut.clear();
}

void Transformation::alloc()
{
    freeAll();
    ctx.memory.cleanUp();

    auto const getNodeSize { [](config::BV bv) -> vk::DeviceSize {
        switch (bv) {
        case config::BV::eDOP14:
            return data_bvh::NodeBvhBinaryDOP14::SCALAR_SIZE;
        case config::BV::eOBB:
            return data_bvh::NodeBvhBinaryOBB::SCALAR_SIZE;
        default:
            break;
        }
        return 0;
    } };

    using bfub = vk::BufferUsageFlagBits;
    lime::AllocRequirements aReq {
        .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal,
        .additionalAlignment = 256,
    };
    vk::BufferCreateInfo cInfo {
        .size = 0,
        .usage = bfub::eStorageBuffer | bfub::eShaderDeviceAddress | bfub::eTransferSrc | bfub::eTransferDst,
    };

    cInfo.size = getNodeSize(config.bv) * metadata.nodeCountTotal;
    buffersOut[Buffer::eBVH] = ctx.memory.alloc(aReq, cInfo, "bvh_transformed");

    cInfo.size = sizeof(u32) * metadata.nodeCountTotal;
    buffersIntermediate[Buffer::eTraversalCounters] = ctx.memory.alloc(aReq, cInfo, "transformation_traversal_counters");

    if (config.bv == config::BV::eOBB) {
        cInfo.size = sizeof(f32) * 3 * 14 * metadata.nodeCountTotal;
        buffersIntermediate[Buffer::eDitoPoints] = ctx.memory.alloc(aReq, cInfo, "transformation_obb_dito_points");
        cInfo.size = sizeof(f32) * 3 * 5 * metadata.nodeCountTotal;
        buffersIntermediate[Buffer::eObb] = ctx.memory.alloc(aReq, cInfo, "transformation_obb");
        cInfo.size = sizeof(u32) * 5;
        buffersIntermediate[Buffer::eScheduler] = ctx.memory.alloc(aReq, cInfo, "transformation_obb_scheduler");

        buffersIntermediate[Buffer::eTimes_TMP] = ctx.memory.alloc(
            { .memoryUsage = lime::DeviceMemoryUsage::eDeviceToHost },
            { .size = sizeof(u64) * 5, .usage = bfub::eStorageBuffer | bfub::eShaderDeviceAddress | bfub::eTransferSrc | bfub::eTransferDst },
            "transformation_times");
    }
}

}
