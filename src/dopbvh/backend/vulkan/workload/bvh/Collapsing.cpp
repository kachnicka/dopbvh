#include "Collapsing.h"

#include "data_bvh.h"

namespace backend::vulkan::bvh {

static u32 CreateSpecializationConstants(vk::PhysicalDevice pd)
{
    auto const prop2 { pd.getProperties2() };
    return prop2.properties.limits.maxComputeWorkGroupSize[0];
}

Collapsing::Collapsing(VCtx ctx)
    : ctx(ctx)
    , timestamps(ctx.d, ctx.pd)
{
}

Bvh Collapsing::GetBVH() const
{
    return {
        .bvh = buffersOut.at(Buffer::eBVH).getDeviceAddress(ctx.d),
        .triangles = buffersOut.at(Buffer::eBVHTriangles).getDeviceAddress(ctx.d),
        .triangleIDs = buffersOut.at(Buffer::eBVHTriangleIDs).getDeviceAddress(ctx.d),
        .nodeCountLeaf = metadata.nodeCountLeaf,
        .nodeCountTotal = metadata.nodeCountTotal,
        .bv = config.bv,
        .layout = Bvh::Layout::eBinaryStandard,
    };
}

void Collapsing::Compute(vk::CommandBuffer commandBuffer, Bvh const& inputBvh, vk::DeviceAddress geometryDescriptor)
{
    // allocate full size based on input bvh
    metadata.nodeCountLeaf = inputBvh.nodeCountLeaf;
    metadata.nodeCountTotal = inputBvh.nodeCountTotal;

    reloadPipelines();
    alloc();
    auto const fillZeros { [](vk::CommandBuffer commandBuffer, auto& buf) {
        commandBuffer.fillBuffer(buf.get(), 0, buf.getSizeInBytes(), 0);
    } };
    fillZeros(commandBuffer, buffersIntermediate[Buffer::eScheduler]);
    fillZeros(commandBuffer, buffersIntermediate[Buffer::eTraversalCounters]);

    timestamps.Reset(commandBuffer);
    vk::MemoryBarrier bufferWriteBarrier { .srcAccessMask = vk::AccessFlagBits::eTransferWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), bufferWriteBarrier, nullptr, nullptr);
    timestamps.Begin(commandBuffer);
    collapse(commandBuffer, inputBvh, geometryDescriptor);
    timestamps.End(commandBuffer);

    vk::MemoryBarrier memoryBarrierCompute { .srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eTransferRead };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), memoryBarrierCompute, nullptr, nullptr);
    commandBuffer.copyBuffer(buffersOut[Buffer::eCollapsedNodeCounts].get(), stagingBuffer.get(), vk::BufferCopy(0, 0, 8));
}

void Collapsing::ReadRuntimeData()
{
    metadata.nodeCountLeaf = static_cast<u32*>(stagingBuffer.getMapping())[1];
    metadata.nodeCountTotal = static_cast<u32*>(stagingBuffer.getMapping())[0] + metadata.nodeCountLeaf + 1;
}

stats::Collapsing Collapsing::GatherStats(BvhStats const& bvhStats)
{
    stats::Collapsing stats;
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

void Collapsing::reloadPipelines()
{
    metadata.workgroupSize = CreateSpecializationConstants(ctx.memory.pd);

    static std::array<vk::SpecializationMapEntry, 1> constexpr entries {
        vk::SpecializationMapEntry { 0, 0, sizeof(u32) },
    };
    vk::SpecializationInfo sInfo { 1, entries.data(), 4, &metadata.workgroupSize };

    switch (config.bv) {
    case config::BV::eAABB:
        pCollapse = { ctx.d, ctx.sCache, "gen_collapse_aabb.comp.spv", sInfo };
        break;
    case config::BV::eDOP14:
        pCollapse = { ctx.d, ctx.sCache, "gen_collapse_dop14.comp.spv", sInfo };
        break;
    case config::BV::eOBB:
        pCollapse = { ctx.d, ctx.sCache, "gen_collapse_obb.comp.spv", sInfo };
        break;
    case config::BV::eNone:
        break;
    }
}

void Collapsing::collapse(vk::CommandBuffer commandBuffer, Bvh const& inputBvh, vk::DeviceAddress geometryDescriptor)
{
    vk::MemoryBarrier memoryBarrierCompute { .srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), memoryBarrierCompute, nullptr, nullptr);

    struct PC {
        u64 bvhAddress { 0 };
        u64 bvhTrianglesAddress { 0 };
        u64 bvhTrianglesIndicesAddress { 0 };
        u64 bvhCollapsedAddress { 0 };
        u64 bvhCollapsedTrianglesAddress { 0 };
        u64 bvhCollapsedTrianglesIndicesAddress { 0 };

        u64 counterAddress { 0 };
        u64 nodeStateAddress { 0 };
        u64 sahCostAddress { 0 };
        u64 leafNodesAddress { 0 };
        u64 newNodeIdAddress { 0 };
        u64 newTriIdAddress { 0 };
        u64 collapsedTreeNodeCountsAddress { 0 };

        u64 schedulerDataAddress { 0 };
        u64 indirectDispatchBufferAddress { 0 };

        u64 dbgAddress { 0 };

        u32 leafNodeCount { 0 };
        u32 maxLeafSize { 0 };
        f32 c_t { 3.f };
        f32 c_i { 2.f };
    } pc;
    pc.bvhAddress = inputBvh.bvh;
    pc.bvhTrianglesAddress = inputBvh.triangles;
    pc.bvhTrianglesIndicesAddress = inputBvh.triangleIDs;
    pc.bvhCollapsedAddress = buffersOut[Buffer::eBVH].getDeviceAddress(ctx.d);
    pc.bvhCollapsedTrianglesAddress = buffersOut[Buffer::eBVHTriangles].getDeviceAddress(ctx.d);
    pc.bvhCollapsedTrianglesIndicesAddress = buffersOut[Buffer::eBVHTriangleIDs].getDeviceAddress(ctx.d);

    pc.schedulerDataAddress = buffersIntermediate[Buffer::eScheduler].getDeviceAddress(ctx.d);
    pc.leafNodeCount = inputBvh.nodeCountLeaf;

    pc.counterAddress = buffersIntermediate[Buffer::eTraversalCounters].getDeviceAddress(ctx.d);
    pc.nodeStateAddress = buffersIntermediate[Buffer::eNodeState].getDeviceAddress(ctx.d);
    pc.sahCostAddress = buffersIntermediate[Buffer::eSAHCost].getDeviceAddress(ctx.d);
    pc.leafNodesAddress = buffersIntermediate[Buffer::eLeafNodes].getDeviceAddress(ctx.d);
    pc.newNodeIdAddress = buffersIntermediate[Buffer::eNewNodeId].getDeviceAddress(ctx.d);
    pc.newTriIdAddress = buffersIntermediate[Buffer::eNewTriId].getDeviceAddress(ctx.d);
    pc.collapsedTreeNodeCountsAddress = buffersOut[Buffer::eCollapsedNodeCounts].getDeviceAddress(ctx.d);

    // pc.indirectDispatchBufferAddress = buffers[BufName::eIndirectDispatchBuffer].getDeviceAddress(ctx.d);
    pc.dbgAddress = geometryDescriptor;

    pc.maxLeafSize = config.maxLeafSize;
    pc.c_t = config.c_t;
    pc.c_i = config.c_i;

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pCollapse.get());
    commandBuffer.pushConstants(pCollapse.layout.pipeline.get(), vk::ShaderStageFlagBits::eCompute, 0, 136 + 8, &pc);
    commandBuffer.dispatch(lime::divCeil(inputBvh.nodeCountLeaf, metadata.workgroupSize), 1, 1);
}

void Collapsing::freeIntermediate()
{
    buffersIntermediate.clear();
    stagingBuffer.reset();
}

void Collapsing::freeAll()
{
    freeIntermediate();
    buffersOut.clear();
}

void Collapsing::alloc()
{
    freeAll();
    ctx.memory.cleanUp();

    auto const getNodeSize { [](config::BV bv) -> vk::DeviceSize {
        switch (bv) {
        case config::BV::eAABB:
            return data_bvh::NodeBvhBinary::SCALAR_SIZE;
        case config::BV::eDOP14:
            return data_bvh::NodeBvhBinaryDOP14::SCALAR_SIZE;
        case config::BV::eOBB:
            return data_bvh::NodeBvhBinaryOBB::SCALAR_SIZE;
            // return data_bvh::NodeBvhBinaryDiTO14Points::SCALAR_SIZE;
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

    cInfo.size = getNodeSize(config.bv) * metadata.nodeCountTotal;
    buffersOut[Buffer::eBVH] = ctx.memory.alloc(aReq, cInfo, "bvh_collapsed");
    cInfo.size = sizeof(f32) * 12 * metadata.nodeCountLeaf;
    buffersOut[Buffer::eBVHTriangles] = ctx.memory.alloc(aReq, cInfo, "bvh_collapsed_triangles");
    cInfo.size = sizeof(u32) * 2 * metadata.nodeCountLeaf;
    buffersOut[Buffer::eBVHTriangleIDs] = ctx.memory.alloc(aReq, cInfo, "bvh_collapsed_triangle_ids");

    cInfo.size = sizeof(u32) * 5;
    buffersIntermediate[Buffer::eScheduler] = ctx.memory.alloc(aReq, cInfo, "collapsing_scheduler");

    cInfo.size = sizeof(u32) * metadata.nodeCountTotal;
    buffersIntermediate[Buffer::eTraversalCounters] = ctx.memory.alloc(aReq, cInfo, "collapsing_traversal_counters");
    buffersIntermediate[Buffer::eNodeState] = ctx.memory.alloc(aReq, cInfo, "collapsing_node_state");
    buffersIntermediate[Buffer::eSAHCost] = ctx.memory.alloc(aReq, cInfo, "collapsing_sah_cost");
    buffersIntermediate[Buffer::eLeafNodes] = ctx.memory.alloc(aReq, cInfo, "collapsing_leaf_nodes");
    buffersIntermediate[Buffer::eNewNodeId] = ctx.memory.alloc(aReq, cInfo, "collapsing_new_node_id");
    buffersIntermediate[Buffer::eNewTriId] = ctx.memory.alloc(aReq, cInfo, "collapsing_new_tri_id");

    cInfo.size = sizeof(u32) * 2;
    buffersOut[Buffer::eCollapsedNodeCounts] = ctx.memory.alloc(aReq, cInfo, "collapsed_node_counts");

    stagingBuffer = ctx.memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eDeviceToHost }, { .size = 8, .usage = vk::BufferUsageFlagBits::eTransferDst }, "collapsing_staging");
}

}
