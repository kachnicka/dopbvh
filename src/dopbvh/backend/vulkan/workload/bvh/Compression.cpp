#include "Compression.h"

#include "data_bvh.h"

namespace backend::vulkan::bvh {

static u32 CreateSpecializationConstants(vk::PhysicalDevice pd)
{
    auto const prop2 { pd.getProperties2() };
    return prop2.properties.limits.maxComputeWorkGroupSize[0];
}

Compression::Compression(VCtx ctx)
    : ctx(ctx)
    , timestamps(ctx.d, ctx.pd)
{
}

Bvh Compression::GetBVH() const
{
    return {
        .bvh = buffersOut.at(Buffer::eBVH).getDeviceAddress(ctx.d),
        .triangles = metadata.bvhTriangles,
        .triangleIDs = metadata.bvhTriangleIDs,
        .bvhAux = bBvhAux.isValid() ? bBvhAux.getDeviceAddress(ctx.d) : 0,
        .nodeCountLeaf = metadata.nodeCountLeaf,
        .nodeCountTotal = metadata.nodeCountTotal,
        .bv = config.bv,
        .layout = config.layout == config::CompressedLayout::eBinaryDOP14Split ? Bvh::Layout::eBinaryCompressed_dop14Split : Bvh::Layout::eBinaryCompressed,
    };
}

void Compression::Compute(vk::CommandBuffer commandBuffer, Bvh const& inputBvh)
{
    metadata.nodeCountLeaf = inputBvh.nodeCountLeaf;
    metadata.nodeCountTotal = (inputBvh.nodeCountTotal - 1) / 2;
    metadata.inputNodeCountTotal = inputBvh.nodeCountTotal;
    metadata.bvhTriangles = inputBvh.triangles;
    metadata.bvhTriangleIDs = inputBvh.triangleIDs;

    reloadPipelines();
    alloc();
    auto const fillZeros { [](vk::CommandBuffer commandBuffer, auto& buf) {
        commandBuffer.fillBuffer(buf.get(), 0, buf.getSizeInBytes(), 0);
    } };
    fillZeros(commandBuffer, buffersIntermediate[Buffer::eScheduler]);

    timestamps.Reset(commandBuffer);
    vk::MemoryBarrier bufferWriteBarrier { .srcAccessMask = vk::AccessFlagBits::eTransferWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), bufferWriteBarrier, nullptr, nullptr);
    timestamps.Begin(commandBuffer);
    compress(commandBuffer, inputBvh);
    timestamps.End(commandBuffer);
}

void Compression::ReadRuntimeData()
{
}

stats::Compression Compression::GatherStats(BvhStats const& bvhStats)
{
    stats::Compression stats;
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

void Compression::reloadPipelines()
{
    metadata.workgroupSize = CreateSpecializationConstants(ctx.pd);

    static std::array<vk::SpecializationMapEntry, 1> constexpr entries {
        vk::SpecializationMapEntry { 0, 0, sizeof(u32) },
    };
    vk::SpecializationInfo sInfo { 1, entries.data(), 4, &metadata.workgroupSize };

    switch (config.bv) {
    case config::BV::eAABB:
        pCompress = { ctx.d, ctx.sCache, "gen_compress_aabb.comp.spv", sInfo };
        break;
    case config::BV::eDOP14:
        switch (config.layout) {
        case config::CompressedLayout::eBinaryStandard:
            pCompress = { ctx.d, ctx.sCache, "gen_compress_dop14_standard.comp.spv", sInfo };
            break;
        case config::CompressedLayout::eBinaryDOP14Split:
            pCompress = { ctx.d, ctx.sCache, "gen_compress_dop14_split.comp.spv", sInfo };
            break;
        }
        break;
    case config::BV::eOBB:
        pCompress = { ctx.d, ctx.sCache, "gen_compress_obb.comp.spv", sInfo };
        break;
    case config::BV::eNone:
        break;
    }
}

void Compression::compress(vk::CommandBuffer commandBuffer, Bvh const& inputBvh)
{
    vk::MemoryBarrier memoryBarrierCompute { .srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), memoryBarrierCompute, nullptr, nullptr);

    struct PC {
        u64 bvhAddress { 0 };
        u64 bvhCompressedAddress { 0 };
        u64 bvhCompressedAuxAddress { 0 };

        u64 schedulerDataAddress { 0 };
        u64 runtimeDataAddress { 0 };

        u64 auxBufferAddress { 0 };
        u32 bvhNodeCount { 0 };
    } pc;

    pc.bvhAddress = inputBvh.bvh;
    pc.bvhCompressedAddress = buffersOut[Buffer::eBVH].getDeviceAddress(ctx.d);
    if (config.layout == config::CompressedLayout::eBinaryDOP14Split)
        pc.bvhCompressedAuxAddress = bBvhAux.getDeviceAddress(ctx.d);

    pc.schedulerDataAddress = buffersIntermediate[Buffer::eScheduler].getDeviceAddress(ctx.d);
    pc.runtimeDataAddress = buffersIntermediate[Buffer::eRuntimeData].getDeviceAddress(ctx.d);

    pc.bvhNodeCount = inputBvh.nodeCountTotal;

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pCompress.get());
    commandBuffer.pushConstants(pCompress.layout.pipeline.get(), vk::ShaderStageFlagBits::eCompute, 0, 52, &pc);
    commandBuffer.dispatch(lime::divCeil(inputBvh.nodeCountLeaf, metadata.workgroupSize), 1, 1);
}

void Compression::freeIntermediate()
{
    buffersIntermediate.clear();
}

void Compression::freeAll()
{
    freeIntermediate();
    buffersOut.clear();
    bBvhAux.reset();
}

void Compression::alloc()
{
    freeAll();
    ctx.memory.cleanUp();

    auto const getNodeSize { [](config::BV bv, config::CompressedLayout layout) -> vk::DeviceSize {
        switch (bv) {
        case config::BV::eAABB:
            return data_bvh::NodeBvhBinaryCompressed::SCALAR_SIZE;
        case config::BV::eDOP14:
            switch (layout) {
            case config::CompressedLayout::eBinaryStandard:
                return data_bvh::NodeBvhBinaryDOP14Compressed::SCALAR_SIZE;
            case config::CompressedLayout::eBinaryDOP14Split:
                return data_bvh::NodeBvhBinaryCompressed::SCALAR_SIZE;
            }
            break;
        case config::BV::eOBB:
            return data_bvh::NodeBvhBinaryOBBCompressed::SCALAR_SIZE;
        case config::BV::eNone:
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

    cInfo.size = getNodeSize(config.bv, config.layout) * metadata.nodeCountTotal;
    buffersOut[Buffer::eBVH] = ctx.memory.alloc(aReq, cInfo, "bvh_compressed");

    if (config.layout == config::CompressedLayout::eBinaryDOP14Split) {
        cInfo.size = data_bvh::NodeBvhBinaryDOP14Compressed_SPLIT::SCALAR_SIZE * metadata.nodeCountTotal;
        bBvhAux = ctx.memory.alloc(aReq, cInfo, "bvh_compressed_split");
    }
    cInfo.size = sizeof(u32) * 5;
    buffersIntermediate[Buffer::eScheduler] = ctx.memory.alloc(aReq, cInfo, "compression_scheduler");
    cInfo.size = sizeof(u32) * 3;
    buffersIntermediate[Buffer::eRuntimeData] = ctx.memory.alloc(aReq, cInfo, "compression_runtime_data");
}

}
