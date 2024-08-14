#include "PLOCpp.h"

#include "../../RadixSort.h"
#include "../../data/Scene.h"
#include "data_bvh.h"
#include "data_plocpp.h"
#include <radix_sort/platforms/vk/radix_sort_vk.h>

namespace backend::vulkan::bvh {

static data_plocpp::SC CreateSpecializationConstants(vk::PhysicalDevice pd, config::PLOC const& config)
{
    auto const prop2 { pd.getProperties2<
        vk::PhysicalDeviceProperties2,
        vk::PhysicalDeviceSubgroupProperties,
        vk::PhysicalDeviceSubgroupSizeControlProperties>() };
    return {
        .sizeWorkgroup = prop2.get<vk::PhysicalDeviceProperties2>().properties.limits.maxComputeWorkGroupSize[0],
        .sizeSubgroup = prop2.get<vk::PhysicalDeviceSubgroupProperties>().subgroupSize,
        .plocRadius = config.radius,
    };
}

PLOCpp::PLOCpp(VCtx ctx)
    : ctx(ctx)
    , timestamps(ctx.d, ctx.pd)
{
}

Bvh PLOCpp::GetBVH() const
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

void PLOCpp::Compute(vk::CommandBuffer commandBuffer, data::Scene const& scene)
{
    metadata.nodeCountLeaf = scene.totalTriangleCount;
    metadata.nodeCountTotal = scene.totalTriangleCount * 2 - 1;

    reloadPipelines();
    alloc();
    auto const fillZeros { [](vk::CommandBuffer commandBuffer, auto& buf) {
        commandBuffer.fillBuffer(buf.get(), 0, buf.getSizeInBytes(), 0);
    } };
    fillZeros(commandBuffer, buffersIntermediate[Buffer::eRuntimeData]);

    timestamps.Reset(commandBuffer);
    timestamps.WriteBeginStamp(commandBuffer);

    initialClusters(commandBuffer, scene);
    timestamps.Write(commandBuffer, Times::Stamp::eInitialClustersAndWoopify);

    sortClusterIDs(commandBuffer);
    timestamps.Write(commandBuffer, Times::Stamp::eSortClusterIDs);

    copySortedClusterIDs(commandBuffer);
    timestamps.Write(commandBuffer, Times::Stamp::eCopySortedClusterIDs);

    vk::MemoryBarrier bufferWriteBarrier { .srcAccessMask = vk::AccessFlagBits::eTransferWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), bufferWriteBarrier, nullptr, nullptr);
    iterationsSingleKernel(commandBuffer);
    timestamps.Write(commandBuffer, Times::Stamp::ePLOCppIterations);

    vk::MemoryBarrier bufferReadBarrier { .srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eTransferRead };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), bufferReadBarrier, nullptr, nullptr);
    commandBuffer.copyBuffer(buffersIntermediate[Buffer::eRuntimeData].get(), stagingBuffer.get(), vk::BufferCopy(36, 0, 4));
}

void PLOCpp::ReadRuntimeData()
{
    metadata.iterationCount = *static_cast<u32*>(stagingBuffer.getMapping());
}

stats::PLOC PLOCpp::GatherStats(BvhStats const& bvhStats)
{
    stats::PLOC stats;

    stats.times = timestamps.GetResultsNs();
    for (auto& t : stats.times) {
        t *= 1e-6f;
        stats.timeTotal += t;
    }

    stats.iterationCount = metadata.iterationCount;

    stats.saIntersect = bvhStats.saIntersect;
    stats.saTraverse = bvhStats.saTraverse;
    stats.costTotal = bvhStats.costIntersect + bvhStats.costTraverse;

    stats.nodeCountTotal = metadata.nodeCountTotal;
    stats.leafSizeMin = bvhStats.leafSizeMin;
    stats.leafSizeMax = bvhStats.leafSizeMax;
    stats.leafSizeAvg = static_cast<f32>(bvhStats.leafSizeSum) / static_cast<f32>(metadata.nodeCountLeaf);

    return stats;
}

void PLOCpp::reloadPipelines()
{
    auto sc { CreateSpecializationConstants(ctx.pd, config) };
    metadata.workgroupSize = sc.sizeWorkgroup;
    metadata.workgroupSizePLOCpp = sc.sizeWorkgroup;

    static std::array<vk::SpecializationMapEntry, 3> constexpr entries {
        vk::SpecializationMapEntry { 0, static_cast<u32>(offsetof(data_plocpp::SC, sizeWorkgroup)), sizeof(u32) },
        vk::SpecializationMapEntry { 1, static_cast<u32>(offsetof(data_plocpp::SC, sizeSubgroup)), sizeof(u32) },
        vk::SpecializationMapEntry { 2, static_cast<u32>(offsetof(data_plocpp::SC, plocRadius)), sizeof(u32) },
    };
    vk::SpecializationInfo sInfo { 1, entries.data(), 4, &metadata.workgroupSize };
    vk::SpecializationInfo sInfoPLOC { 3, entries.data(), 12, &sc };

    pCopySortedClusterIDs = { ctx.d, ctx.sCache, "gen_plocpp_CopyClusterIDs.comp.spv", sInfo };
    switch (config.bv) {
    case config::BV::eAABB:
        pInitialClusters = { ctx.d, ctx.sCache, "gen_plocpp_aabb_InitialClusters.comp.spv", sInfo };
        pPLOCppIterations = { ctx.d, ctx.sCache, "gen_plocpp_aabb_PLOCpp.comp.spv", sInfoPLOC };
        break;
    case config::BV::eDOP14:
        pInitialClusters = { ctx.d, ctx.sCache, "gen_plocpp_dop14_InitialClusters.comp.spv", sInfo };
        // TODO: this value is compatible with used hw (based on shared memory size), but should be queried and computed in runtime
        metadata.workgroupSizePLOCpp = 736u;
        sc.sizeWorkgroup = metadata.workgroupSizePLOCpp;
        pPLOCppIterations = { ctx.d, ctx.sCache, "gen_plocpp_dop14_PLOCpp.comp.spv", sInfoPLOC };
        break;
    case config::BV::eOBB:
        pInitialClusters = { ctx.d, ctx.sCache, "gen_plocpp_obb_InitialClusters.comp.spv", sInfo };
        metadata.workgroupSizePLOCpp = 256u;
        sc.sizeWorkgroup = metadata.workgroupSizePLOCpp;
        pPLOCppIterations = { ctx.d, ctx.sCache, "gen_plocpp_obb_PLOCpp.comp.spv", sInfoPLOC };
        break;
    case config::BV::eNone:
        break;
    }
}

void PLOCpp::initialClusters(vk::CommandBuffer commandBuffer, data::Scene const& scene)
{
    auto const cubedAabb { scene.aabb.GetCubed() };
    data_plocpp::PC_MortonGlobal pcGlobal {
        .sceneAabbCubedMin = { cubedAabb.min.x, cubedAabb.min.y, cubedAabb.min.z },
        .sceneAabbNormalizationScale = 1.f / (cubedAabb.max - cubedAabb.min).x,
        .mortonAddress = buffersIntermediate[Buffer::eRadixEven].getDeviceAddress(ctx.d),
        .bvhAddress = buffersOut[Buffer::eBVH].getDeviceAddress(ctx.d),
        .bvhTrianglesAddress = buffersOut[Buffer::eBVHTriangles].getDeviceAddress(ctx.d),
        .bvhTriangleIndicesAddress = buffersOut[Buffer::eBVHTriangleIDs].getDeviceAddress(ctx.d),

        .auxBufferAddress = 0,
        // .auxBufferAddress = buffersIntermediate[Buffer::eDbgBuffer].getDeviceAddress(ctx.d),
    };

    data_plocpp::PC_MortonPerGeometry pcPerGeometry {
        .idxAddress = 0,
        .vtxAddress = 0,
        .globalTriangleIdBase = 0,
        .sceneNodeId = 0,
        .triangleCount = 0,
    };

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pInitialClusters.get());
    commandBuffer.pushConstants(
        pInitialClusters.layout.pipeline.get(),
        vk::ShaderStageFlagBits::eCompute,
        0, data_plocpp::PC_MortonGlobal::SCALAR_SIZE,
        &pcGlobal);

    // TODO: probably should iterate over nodes and pass transform
    //  to not miss instances and to match scene final transformation
    for (auto const& gId : scene.geometries) {
        auto const& g { scene.data->geometries[gId] };

        pcPerGeometry.idxAddress = g.indexBuffer.getDeviceAddress(ctx.d);
        pcPerGeometry.vtxAddress = g.vertexBuffer.getDeviceAddress(ctx.d);
        pcPerGeometry.sceneNodeId = gId.get();
        pcPerGeometry.triangleCount = g.indexCount / 3;

        commandBuffer.pushConstants(
            pInitialClusters.layout.pipeline.get(),
            vk::ShaderStageFlagBits::eCompute,
            data_plocpp::PC_MortonGlobal::SCALAR_SIZE, data_plocpp::PC_MortonPerGeometry::SCALAR_SIZE,
            &pcPerGeometry);
        commandBuffer.dispatch(lime::divCeil(pcPerGeometry.triangleCount, metadata.workgroupSize), 1, 1);

        pcPerGeometry.globalTriangleIdBase += pcPerGeometry.triangleCount;
    }
}

void PLOCpp::sortClusterIDs(vk::CommandBuffer commandBuffer)
{
    vk::MemoryBarrier memoryBarrierCompute { .srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), memoryBarrierCompute, nullptr, nullptr);

    radix_sort_vk_memory_requirements_t radixSortMemory;
    radix_sort_vk_get_memory_requirements(FuchsiaRadixSort::radixSort, metadata.nodeCountLeaf, &radixSortMemory);

    radix_sort_vk_sort_info_t radixInfo;
    radixInfo.ext = nullptr;
    radixInfo.key_bits = 32;
    radixInfo.count = metadata.nodeCountLeaf;
    radixInfo.keyvals_even = { buffersIntermediate[Buffer::eRadixEven].get(), 0, radixSortMemory.keyvals_size };
    radixInfo.keyvals_odd = { buffersIntermediate[Buffer::eRadixOdd].get(), 0, radixSortMemory.keyvals_size };
    radixInfo.internal = { buffersIntermediate[Buffer::eRadixInternal].get(), 0, radixSortMemory.internal_size };

    VkDescriptorBufferInfo results;
    radix_sort_vk_sort(FuchsiaRadixSort::radixSort, &radixInfo, ctx.d, commandBuffer, &results);

    auto& even { buffersIntermediate[Buffer::eRadixEven] };
    auto& odd { buffersIntermediate[Buffer::eRadixOdd] };
    nodeBuffer1Address = (vk::Buffer(results.buffer) == even.get() ? even : odd).getDeviceAddress(ctx.d);
    nodeBuffer0Address = (vk::Buffer(results.buffer) == even.get() ? odd : even).getDeviceAddress(ctx.d);
}

void PLOCpp::copySortedClusterIDs(vk::CommandBuffer commandBuffer)
{
    vk::MemoryBarrier memoryBarrierCompute { .srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), memoryBarrierCompute, nullptr, nullptr);

    data_plocpp::PC_CopySortedNodeIds pc {
        .mortonAddress = nodeBuffer1Address,
        .nodeIdAddress = nodeBuffer0Address,
        .clusterCount = metadata.nodeCountLeaf,
    };

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pCopySortedClusterIDs.get());
    commandBuffer.pushConstants(pCopySortedClusterIDs.layout.pipeline.get(), vk::ShaderStageFlagBits::eCompute, 0, data_plocpp::PC_CopySortedNodeIds::SCALAR_SIZE, &pc);
    commandBuffer.dispatch(lime::divCeil(metadata.nodeCountLeaf, metadata.workgroupSize), 1, 1);
}

void PLOCpp::iterationsSingleKernel(vk::CommandBuffer commandBuffer)
{
    vk::MemoryBarrier memoryBarrierCompute { .srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), memoryBarrierCompute, nullptr, nullptr);

    data_plocpp::PC_PlocppIteration pc {
        .bvhAddress = buffersOut[Buffer::eBVH].getDeviceAddress(ctx.d),
        .nodeId0Address = nodeBuffer0Address,
        .nodeId1Address = nodeBuffer1Address,
        .dlWorkBufAddress = buffersIntermediate[Buffer::eDecoupledLookBack].getDeviceAddress(ctx.d),
        .runtimeDataAddress = buffersIntermediate[Buffer::eRuntimeData].getDeviceAddress(ctx.d),

        // .auxBufferAddress = 0,
        .auxBufferAddress = buffersIntermediate[Buffer::eDebug].getDeviceAddress(ctx.d),

        .clusterCount = metadata.nodeCountLeaf,
    };

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pPLOCppIterations.get());
    commandBuffer.pushConstants(
        pPLOCppIterations.layout.pipeline.get(),
        vk::ShaderStageFlagBits::eCompute,
        0, data_plocpp::PC_PlocppIteration::SCALAR_SIZE,
        &pc);

    commandBuffer.dispatch(1024, 1, 1);
}

void PLOCpp::freeIntermediate()
{
    buffersIntermediate.clear();
    nodeBuffer0Address = 0;
    nodeBuffer1Address = 0;
    stagingBuffer.reset();
}

void PLOCpp::freeAll()
{
    freeIntermediate();
    buffersOut.clear();
}

void PLOCpp::alloc()
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
            return data_bvh::NodeBvhBinaryDiTO14Points::SCALAR_SIZE;
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
    buffersOut[Buffer::eBVH] = ctx.memory.alloc(aReq, cInfo, "bvh_plocpp");
    cInfo.size = sizeof(f32) * 12 * metadata.nodeCountLeaf;
    buffersOut[Buffer::eBVHTriangles] = ctx.memory.alloc(aReq, cInfo, "bvh_plocpp_triangles");
    cInfo.size = sizeof(u32) * 2 * metadata.nodeCountLeaf;
    buffersOut[Buffer::eBVHTriangleIDs] = ctx.memory.alloc(aReq, cInfo, "bvh_plocpp_triangle_ids");

    radix_sort_vk_memory_requirements_t radixSortMemory;
    radix_sort_vk_get_memory_requirements(FuchsiaRadixSort::radixSort, metadata.nodeCountLeaf, &radixSortMemory);
    cInfo.size = radixSortMemory.keyvals_size;
    buffersIntermediate[Buffer::eRadixEven] = ctx.memory.alloc(aReq, cInfo, "plocpp_radix_even");
    buffersIntermediate[Buffer::eRadixOdd] = ctx.memory.alloc(aReq, cInfo, "plocpp_radix_odd");
    cInfo.size = radixSortMemory.internal_size;
    buffersIntermediate[Buffer::eRadixInternal] = ctx.memory.alloc(aReq, cInfo, "plocpp_radix_internal");

    cInfo.size = sizeof(u32) * 10;
    buffersIntermediate[Buffer::eRuntimeData] = ctx.memory.alloc(aReq, cInfo, "plocpp_runtime_data");

    cInfo.size = sizeof(u32) * 2 * lime::divCeil(metadata.nodeCountLeaf, metadata.workgroupSizePLOCpp - 4 * config.radius);
    buffersIntermediate[Buffer::eDecoupledLookBack] = ctx.memory.alloc(aReq, cInfo, "plocpp_decoupled_lookback");

    cInfo.size = sizeof(f32) * metadata.nodeCountTotal * 2;
    buffersIntermediate[Buffer::eDebug] = ctx.memory.alloc(aReq, cInfo, "plocpp_debug");

    stagingBuffer = ctx.memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eDeviceToHost }, { .size = 4, .usage = vk::BufferUsageFlagBits::eTransferDst }, "plocpp_staging");
}
}
