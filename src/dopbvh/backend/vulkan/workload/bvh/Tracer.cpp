#include "Tracer.h"

#include "data_bvh.h"

namespace backend::vulkan::bvh {
using bfub = vk::BufferUsageFlagBits;

static u32 CreateSpecializationConstants(vk::PhysicalDevice pd)
{
    auto const prop2 { pd.getProperties2() };
    return prop2.properties.limits.maxComputeWorkGroupSize[0];
}

Tracer::Tracer(VCtx ctx)
    : ctx(ctx)
    , timestamp(ctx.d, ctx.pd)
{
    allocStatic();
}

void Tracer::Trace(vk::CommandBuffer commandBuffer, config::Tracer const& traceCfg, TraceRuntime const& trt, Bvh const& inputBvh)
{
    if (traceCfg != config) {
        config = traceCfg;
        metadata.reloadPipelines = true;
    }
    if (inputBvh.layout != metadata.bvhMemoryLayout) {
        metadata.bvhMemoryLayout = inputBvh.layout;
        metadata.reloadPipelines = true;
    }
    if (config.bv == config::BV::eNone)
        return;

    if (metadata.reloadPipelines) {
        reloadPipelines();
        metadata.reloadPipelines = false;
    }
    if (u32 rayCount { trt.x * trt.y }; rayCount > metadata.rayCount) {
        freeRayBuffers();
        allocRayBuffers(rayCount);
    }
    dSetUpdate(trt.targetImageView, trt.camera);

    lime::debug::BeginDebugLabel(commandBuffer, "path tracing compute", lime::debug::LabelColor::eBordeaux);

    // TODO:use enum
    switch (config.traceMode) {
    case 0:
        if (config.useSeparateKernels)
            trace_separate(commandBuffer, trt, inputBvh);
        else
            trace_joined(commandBuffer, trt, inputBvh);
        break;
    case 1:
        trace_separate(commandBuffer, trt, inputBvh, [this](vk::CommandBuffer commandBuffer) {
            if (config.bv == config::BV::eDOP14 && metadata.bvhMemoryLayout != Bvh::Layout::eBinaryCompressed_dop14Split)
                return;
            commandBuffer.pushConstants(pTrace.getLayout(), vk::ShaderStageFlagBits::eCompute, data_bvh::PC_PT::SCALAR_SIZE, 4, &config.bvDepth);
        });
        break;
    case 2:
        trace_joined(commandBuffer, trt, inputBvh);
    default:
        break;
    }
    lime::debug::EndDebugLabel(commandBuffer);

    readTs = true;
}

void Tracer::trace_joined(vk::CommandBuffer commandBuffer, TraceRuntime const& trt, Bvh const& inputBvh)
{
    std::vector<vk::DescriptorSet> desc_set { dSet };
    data_bvh::PC_PT pc {
        .dirLight = { 0.f },

        .schedulerDataAddress = bScheduler.getDeviceAddress(ctx.d),
        .ptStatsAddress = bStats.getDeviceAddress(ctx.d),

        .rayBufferMetadataAddress = bRayMetadata.getDeviceAddress(ctx.d),
        .rayBuffer0Address = bRay[0].getDeviceAddress(ctx.d),
        .rayBuffer1Address = bRay[1].getDeviceAddress(ctx.d),
        .rayPayload0Address = bRayPayload[0].getDeviceAddress(ctx.d),
        .rayPayload1Address = bRayPayload[1].getDeviceAddress(ctx.d),
        .rayTraceResultAddress = bTraceResult.getDeviceAddress(ctx.d),

        .geometryDescriptorAddress = trt.geometryDescriptorAddress,
        .bvhAddress = inputBvh.bvh,
        .bvhTrianglesAddress = inputBvh.triangles,
        .bvhTriangleIndicesAddress = inputBvh.triangleIDs,
        .auxBufferAddress = inputBvh.bvhAux,

        .samplesComputed = trt.samples.computed,
        .samplesToComputeThisFrame = trt.samples.toCompute,
    };
    memcpy(&pc.dirLight, &dirLight_TMP, sizeof(dirLight_TMP));

    commandBuffer.fillBuffer(bStats.get(), 0, 8, uint32_t(-1));
    commandBuffer.fillBuffer(bStats.get(), 8, bStats.getSizeInBytes() - 8, 0);
    commandBuffer.fillBuffer(bScheduler.get(), 0, bScheduler.getSizeInBytes(), 0);
    vk::MemoryBarrier writeMemoryBarrier { .srcAccessMask = vk::AccessFlagBits::eTransferWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), writeMemoryBarrier, nullptr, nullptr);

    timestamp.Reset(commandBuffer);
    timestamp.Begin(commandBuffer);
    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pTrace.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pTrace.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pTrace.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    commandBuffer.dispatch(config.workgroupCount, 1, 1);
    timestamp.End(commandBuffer);

    vk::MemoryBarrier memoryBarrierCompute { .srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eTransferRead };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), memoryBarrierCompute, nullptr, nullptr);
    commandBuffer.copyBuffer(bStats.get(), bStaging.get(), vk::BufferCopy(0, 0, bStats.getSizeInBytes()));
}

void Tracer::trace_separate(vk::CommandBuffer commandBuffer, TraceRuntime const& trt, Bvh const& inputBvh, std::function<void(vk::CommandBuffer)> const& additionalCommands)
{
    std::vector<vk::DescriptorSet> desc_set { dSet };
    data_bvh::PC_PT pc {
        .dirLight = { 0.f },

        .schedulerDataAddress = bScheduler.getDeviceAddress(ctx.d),
        .ptStatsAddress = bStats.getDeviceAddress(ctx.d),

        .rayBufferMetadataAddress = bRayMetadata.getDeviceAddress(ctx.d),
        .rayBuffer0Address = bRay[0].getDeviceAddress(ctx.d),
        .rayBuffer1Address = bRay[1].getDeviceAddress(ctx.d),
        .rayPayload0Address = bRayPayload[0].getDeviceAddress(ctx.d),
        .rayPayload1Address = bRayPayload[1].getDeviceAddress(ctx.d),
        .rayTraceResultAddress = bTraceResult.getDeviceAddress(ctx.d),

        .geometryDescriptorAddress = trt.geometryDescriptorAddress,
        .bvhAddress = inputBvh.bvh,
        .bvhTrianglesAddress = inputBvh.triangles,
        .bvhTriangleIndicesAddress = inputBvh.triangleIDs,
        .auxBufferAddress = inputBvh.bvhAux,

        .samplesComputed = trt.samples.computed,
        .samplesToComputeThisFrame = trt.samples.toCompute,
    };
    memcpy(&pc.dirLight, &dirLight_TMP, sizeof(dirLight_TMP));

    static vk::MemoryBarrier writeBarrierBefore { .srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eTransferWrite };
    static vk::MemoryBarrier writeBarrierAfter { .srcAccessMask = vk::AccessFlagBits::eTransferWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };
    static vk::MemoryBarrier computeBarrier { .srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };

    u32 depth { 0 };
    commandBuffer.fillBuffer(bStats.get(), 0, bStats.getSizeInBytes(), 0);
    commandBuffer.fillBuffer(bStats.get(), 128, 8, uint32_t(-1));
    commandBuffer.fillBuffer(bRayMetadata.get(), 0, bRayMetadata.getSizeInBytes(), 0);
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), writeBarrierAfter, nullptr, nullptr);

    timestamp.Reset(commandBuffer);
    timestamp.Begin(commandBuffer);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pPrimaryRayGen.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pPrimaryRayGen.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pPrimaryRayGen.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    commandBuffer.dispatch(lime::divCeil(trt.x, 32u), lime::divCeil(trt.y, 32u), 1);

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), writeBarrierBefore, nullptr, nullptr);
    commandBuffer.copyBuffer(bRayMetadata.get(), bStats.get(), vk::BufferCopy(0, depth * 16 + 8, 4));
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), writeBarrierAfter, nullptr, nullptr);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pTrace.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pTrace.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pTrace.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    additionalCommands(commandBuffer);
    commandBuffer.dispatch(config.workgroupCount, 1, 1);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pShadeAndCast.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pShadeAndCast.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pShadeAndCast.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), computeBarrier, nullptr, nullptr);
    commandBuffer.dispatch(lime::divCeil(metadata.rayCount, 1024u), 1, 1);
    depth++;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), writeBarrierBefore, nullptr, nullptr);
    commandBuffer.fillBuffer(bRayMetadata.get(), 8, 4, depth);
    commandBuffer.fillBuffer(bRayMetadata.get(), 0, 8, 0);
    commandBuffer.fillBuffer(bStats.get(), 128, 8, uint32_t(-1));
    commandBuffer.copyBuffer(bRayMetadata.get(), bStats.get(), vk::BufferCopy(16, depth * 16 + 8, 4));
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), writeBarrierAfter, nullptr, nullptr);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pTrace.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pTrace.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pTrace.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    additionalCommands(commandBuffer);
    commandBuffer.dispatch(config.workgroupCount, 1, 1);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pShadeAndCast.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pShadeAndCast.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pShadeAndCast.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), computeBarrier, nullptr, nullptr);
    commandBuffer.dispatch(lime::divCeil(metadata.rayCount, 1024u), 1, 1);
    depth++;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), writeBarrierBefore, nullptr, nullptr);
    commandBuffer.fillBuffer(bRayMetadata.get(), 8, 4, depth);
    commandBuffer.fillBuffer(bRayMetadata.get(), 16, 8, 0);
    commandBuffer.fillBuffer(bStats.get(), 128, 8, uint32_t(-1));
    commandBuffer.copyBuffer(bRayMetadata.get(), bStats.get(), vk::BufferCopy(0, depth * 16 + 8, 4));
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), writeBarrierAfter, nullptr, nullptr);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pTrace.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pTrace.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pTrace.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    additionalCommands(commandBuffer);
    commandBuffer.dispatch(config.workgroupCount, 1, 1);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pShadeAndCast.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pShadeAndCast.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pShadeAndCast.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), computeBarrier, nullptr, nullptr);
    commandBuffer.dispatch(lime::divCeil(metadata.rayCount, 1024u), 1, 1);
    depth++;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), writeBarrierBefore, nullptr, nullptr);
    commandBuffer.fillBuffer(bRayMetadata.get(), 8, 4, depth);
    commandBuffer.fillBuffer(bRayMetadata.get(), 0, 8, 0);
    commandBuffer.fillBuffer(bStats.get(), 128, 8, uint32_t(-1));
    commandBuffer.copyBuffer(bRayMetadata.get(), bStats.get(), vk::BufferCopy(16, depth * 16 + 8, 4));
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), writeBarrierAfter, nullptr, nullptr);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pTrace.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pTrace.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pTrace.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    additionalCommands(commandBuffer);
    commandBuffer.dispatch(config.workgroupCount, 1, 1);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pShadeAndCast.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pShadeAndCast.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pShadeAndCast.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), computeBarrier, nullptr, nullptr);
    commandBuffer.dispatch(lime::divCeil(metadata.rayCount, 1024u), 1, 1);
    depth++;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), writeBarrierBefore, nullptr, nullptr);
    commandBuffer.fillBuffer(bRayMetadata.get(), 8, 4, depth);
    commandBuffer.fillBuffer(bRayMetadata.get(), 16, 8, 0);
    commandBuffer.fillBuffer(bStats.get(), 128, 8, uint32_t(-1));
    commandBuffer.copyBuffer(bRayMetadata.get(), bStats.get(), vk::BufferCopy(0, depth * 16 + 8, 4));
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), writeBarrierAfter, nullptr, nullptr);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pTrace.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pTrace.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pTrace.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    additionalCommands(commandBuffer);
    commandBuffer.dispatch(config.workgroupCount, 1, 1);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pShadeAndCast.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pShadeAndCast.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pShadeAndCast.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), computeBarrier, nullptr, nullptr);
    commandBuffer.dispatch(lime::divCeil(metadata.rayCount, 1024u), 1, 1);
    depth++;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), writeBarrierBefore, nullptr, nullptr);
    commandBuffer.fillBuffer(bRayMetadata.get(), 8, 4, depth);
    commandBuffer.fillBuffer(bRayMetadata.get(), 0, 8, 0);
    commandBuffer.fillBuffer(bStats.get(), 128, 8, uint32_t(-1));
    commandBuffer.copyBuffer(bRayMetadata.get(), bStats.get(), vk::BufferCopy(16, depth * 16 + 8, 4));
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), writeBarrierAfter, nullptr, nullptr);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pTrace.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pTrace.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pTrace.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    additionalCommands(commandBuffer);
    commandBuffer.dispatch(config.workgroupCount, 1, 1);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pShadeAndCast.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pShadeAndCast.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pShadeAndCast.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), computeBarrier, nullptr, nullptr);
    commandBuffer.dispatch(lime::divCeil(metadata.rayCount, 1024u), 1, 1);
    depth++;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), writeBarrierBefore, nullptr, nullptr);
    commandBuffer.fillBuffer(bRayMetadata.get(), 8, 4, depth);
    commandBuffer.fillBuffer(bRayMetadata.get(), 16, 8, 0);
    commandBuffer.fillBuffer(bStats.get(), 128, 8, uint32_t(-1));
    commandBuffer.copyBuffer(bRayMetadata.get(), bStats.get(), vk::BufferCopy(0, depth * 16 + 8, 4));
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), writeBarrierAfter, nullptr, nullptr);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pTrace.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pTrace.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pTrace.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    additionalCommands(commandBuffer);
    commandBuffer.dispatch(config.workgroupCount, 1, 1);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pShadeAndCast.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pShadeAndCast.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pShadeAndCast.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), computeBarrier, nullptr, nullptr);
    commandBuffer.dispatch(lime::divCeil(metadata.rayCount, 1024u), 1, 1);
    depth++;

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), writeBarrierBefore, nullptr, nullptr);
    commandBuffer.fillBuffer(bRayMetadata.get(), 8, 4, depth);
    commandBuffer.fillBuffer(bRayMetadata.get(), 0, 8, 0);
    commandBuffer.fillBuffer(bStats.get(), 128, 8, uint32_t(-1));
    commandBuffer.copyBuffer(bRayMetadata.get(), bStats.get(), vk::BufferCopy(16, depth * 16 + 8, 4));

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), writeBarrierAfter, nullptr, nullptr);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pTrace.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pTrace.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pTrace.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    additionalCommands(commandBuffer);
    commandBuffer.dispatch(config.workgroupCount, 1, 1);

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, pShadeAndCast.get());
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pShadeAndCast.getLayout(), 0, desc_set, { 0 });
    commandBuffer.pushConstants(pShadeAndCast.getLayout(), vk::ShaderStageFlagBits::eCompute, 0, data_bvh::PC_PT::SCALAR_SIZE, &pc);
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlags(), computeBarrier, nullptr, nullptr);
    commandBuffer.dispatch(lime::divCeil(metadata.rayCount, 1024u), 1, 1);

    timestamp.End(commandBuffer);

    vk::MemoryBarrier memoryBarrierCompute { .srcAccessMask = vk::AccessFlagBits::eShaderWrite, .dstAccessMask = vk::AccessFlagBits::eTransferRead };
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer, vk::DependencyFlags(), memoryBarrierCompute, nullptr, nullptr);
    commandBuffer.copyBuffer(bStats.get(), bStaging.get(), vk::BufferCopy(0, 0, bStats.getSizeInBytes()));
}

stats::Trace Tracer::GetStats() const
{
    stats::Trace result;

    static auto const timestampToMs = [pd = ctx.pd](u64 stamp) -> f32 {
        static auto const timestampValidBits { pd.getQueueFamilyProperties()[0].timestampValidBits };
        static auto const timestampPeriod { pd.getProperties().limits.timestampPeriod };

        if (timestampValidBits < sizeof(u64) * 8)
            stamp = stamp & ~(static_cast<u64>(-1) << timestampValidBits);
        return static_cast<f32>(stamp) * timestampPeriod * 1e-6f;
    };
    data_bvh::PT_Stats const& stats { *static_cast<data_bvh::PT_Stats*>(bStaging.getMapping()) };

    for (int i = 0; i < 8; i++) {
        result.data[i].rayCount = stats.times[i].rayCount;
        result.data[i].traceTimeMs = timestampToMs(stats.times[i].timer);
    }

    return result;
}

void Tracer::reloadPipelines()
{
    metadata.workgroupSize = CreateSpecializationConstants(ctx.pd);

    static std::array<vk::SpecializationMapEntry, 1> constexpr entries {
        vk::SpecializationMapEntry { 0, 0, sizeof(uint32_t) },
    };
    vk::SpecializationInfo sInfo { 1, entries.data(), 4, &metadata.workgroupSize };

    // TODO:use enum
    switch (config.traceMode) {
    case 0:
        if (config.useSeparateKernels) {
            pPrimaryRayGen = { ctx.d, ctx.sCache, "gen_ptrace_primary_rays.comp.spv", sInfo };
            pShadeAndCast = { ctx.d, ctx.sCache, "gen_ptrace_shadeAndCast.comp.spv", sInfo };
            switch (config.bv) {
            case config::BV::eAABB:
                pTrace = { ctx.d, ctx.sCache, "gen_ptrace_aabb_sep.comp.spv", sInfo };
                break;
            case config::BV::eDOP14:
                if (metadata.bvhMemoryLayout == Bvh::Layout::eBinaryCompressed_dop14Split)
                    pTrace = { ctx.d, ctx.sCache, "gen_ptrace_dop14_split_sep.comp.spv", sInfo };
                else
                    pTrace = { ctx.d, ctx.sCache, "gen_ptrace_dop14_standard_sep.comp.spv", sInfo };
                break;
            case config::BV::eOBB:
                pTrace = { ctx.d, ctx.sCache, "gen_ptrace_obb_sep.comp.spv", sInfo };
                break;
            default:
                pTrace = {};
            }
        } else {
            pPrimaryRayGen = {};
            pShadeAndCast = {};
            switch (config.bv) {
            case config::BV::eAABB:
                pTrace = { ctx.d, ctx.sCache, "gen_ptrace_aabb.comp.spv", sInfo };
                break;
            case config::BV::eDOP14:
                if (metadata.bvhMemoryLayout == Bvh::Layout::eBinaryCompressed_dop14Split)
                    pTrace = { ctx.d, ctx.sCache, "gen_ptrace_dop14_split.comp.spv", sInfo };
                else
                    pTrace = { ctx.d, ctx.sCache, "gen_ptrace_dop14_standard.comp.spv", sInfo };
                break;
            case config::BV::eOBB:
                pTrace = { ctx.d, ctx.sCache, "gen_ptrace_obb.comp.spv", sInfo };
                break;
            default:
                pTrace = {};
            }
        }
        break;
    case 1: {
        pPrimaryRayGen = { ctx.d, ctx.sCache, "gen_ptrace_primary_rays.comp.spv", sInfo };
        pShadeAndCast = { ctx.d, ctx.sCache, "gen_ptrace_shadeAndCast_bv.comp.spv", sInfo };
        switch (config.bv) {
        case config::BV::eAABB:
            pTrace = { ctx.d, ctx.sCache, "gen_ptrace_aabb_sep_bv.comp.spv", sInfo };
            break;
        case config::BV::eDOP14:
            if (metadata.bvhMemoryLayout == Bvh::Layout::eBinaryCompressed_dop14Split)
                pTrace = { ctx.d, ctx.sCache, "gen_ptrace_dop14_split_sep_bv.comp.spv", sInfo };
            else {
                berry::Log::warn("BV rendering for 14-DOP naive layout is not supported, try split layout instead.");
                pTrace = { ctx.d, ctx.sCache, "gen_ptrace_dop14_standard_sep.comp.spv", sInfo };
            }
            break;
        case config::BV::eOBB:
            pTrace = { ctx.d, ctx.sCache, "gen_ptrace_obb_sep_bv.comp.spv", sInfo };
            break;
        default:
            pTrace = {};
        }
    } break;
    case 2: {
        pPrimaryRayGen = {};
        pShadeAndCast = {};
        switch (config.bv) {
        case config::BV::eAABB:
            pTrace = { ctx.d, ctx.sCache, "gen_ptrace_aabb_heat.comp.spv", sInfo };
            break;
        case config::BV::eDOP14:
            if (metadata.bvhMemoryLayout == Bvh::Layout::eBinaryCompressed_dop14Split)
                pTrace = { ctx.d, ctx.sCache, "gen_ptrace_dop14_split_heat.comp.spv", sInfo };
            else {
                berry::Log::warn("Heatmap rendering for 14-DOP naive layout is not supported, try split layout instead.");
            }
            break;
        case config::BV::eOBB:
            pTrace = { ctx.d, ctx.sCache, "gen_ptrace_obb_heat.comp.spv", sInfo };
            break;
        default:
            pTrace = {};
        }
    } break;
    default:
        break;
    }

    std::vector<vk::DescriptorPoolSize> poolSizes {
        { vk::DescriptorType::eStorageImage, 1 },
        { vk::DescriptorType::eUniformBuffer, 1 },
    };
    vk::DescriptorPoolCreateInfo cInfo;
    cInfo.maxSets = 1;
    cInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    cInfo.pPoolSizes = poolSizes.data();
    dPool = lime::check(ctx.d.createDescriptorPoolUnique(cInfo));

    vk::DescriptorSetAllocateInfo allocInfo;
    allocInfo.descriptorPool = dPool.get();
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &pTrace.layout.dSet[0].get();
    dSet = lime::check(ctx.d.allocateDescriptorSets(allocInfo)).back();
}

void Tracer::dSetUpdate(vk::ImageView targetImageView, lime::Buffer::Detail const& camera) const
{
    vk::DescriptorImageInfo dImage[1];
    dImage[0].imageView = targetImageView;
    dImage[0].imageLayout = vk::ImageLayout::eGeneral;

    vk::DescriptorBufferInfo dBuffer[1];
    dBuffer[0].buffer = camera.resource;
    dBuffer[0].offset = camera.offset;
    dBuffer[0].range = camera.size;

    vk::WriteDescriptorSet dWrite[2];
    dWrite[0].dstSet = dSet;
    dWrite[0].dstBinding = 0;
    dWrite[0].descriptorCount = 1;
    dWrite[0].descriptorType = vk::DescriptorType::eStorageImage;
    dWrite[0].pImageInfo = dImage;

    dWrite[1].dstSet = dSet;
    dWrite[1].dstBinding = 1;
    dWrite[1].descriptorCount = 1;
    dWrite[1].descriptorType = vk::DescriptorType::eUniformBufferDynamic;
    dWrite[1].pBufferInfo = dBuffer;

    ctx.d.updateDescriptorSets(2, dWrite, 0, nullptr);
}

void Tracer::allocRayBuffers(uint32_t rayCount)
{
    lime::AllocRequirements aReq {
        .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal,
    };
    vk::BufferCreateInfo cInfo {
        .size = 0,
        .usage = bfub::eStorageBuffer | bfub::eShaderDeviceAddress,
    };

    metadata.rayCount = rayCount;

    cInfo.size = metadata.rayCount * data_bvh::Ray::SCALAR_SIZE;
    bRay[0] = ctx.memory.alloc(aReq, cInfo, "tracer_ray_buffer_0");
    bRay[1] = ctx.memory.alloc(aReq, cInfo, "tracer_ray_buffer_1");

    cInfo.size = metadata.rayCount * data_bvh::RayPayload::SCALAR_SIZE;
    bRayPayload[0] = ctx.memory.alloc(aReq, cInfo, "tracer_ray_payload_0");
    bRayPayload[1] = ctx.memory.alloc(aReq, cInfo, "tracer_ray_payload_1");

    cInfo.size = metadata.rayCount * data_bvh::RayTraceResult::SCALAR_SIZE;
    bTraceResult = ctx.memory.alloc(aReq, cInfo, "tracer_result");
}

void Tracer::allocStatic()
{
    if (!bScheduler.isValid()) {
        bScheduler = ctx.memory.alloc(
            { .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal },
            {
                .size = 5 * sizeof(u32),
                .usage = bfub::eStorageBuffer | bfub::eShaderDeviceAddress | bfub::eTransferDst,
            },
            "tracer_scheduler");
    }
    if (!bRayMetadata.isValid()) {
        bRayMetadata = ctx.memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal },
            {
                .size = 2 * data_bvh::RayBufferMetadata::SCALAR_SIZE,
                .usage = bfub::eStorageBuffer | bfub::eShaderDeviceAddress | bfub::eTransferDst | bfub::eTransferSrc,
            },
            "tracer_ray_metadata");
    }
    if (!bStats.isValid()) {
        bStats = ctx.memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal },
            {
                .size = data_bvh::PT_Stats::SCALAR_SIZE,
                .usage = bfub::eStorageBuffer | bfub::eShaderDeviceAddress | bfub::eTransferDst | bfub::eTransferSrc,
            },

            "tracer_stats");
    }
    if (!bStaging.isValid()) {
        bStaging = ctx.memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eDeviceToHost },
            {
                .size = data_bvh::PT_Stats::SCALAR_SIZE,
                .usage = bfub::eTransferDst,
            },
            "tracer_staging");
    }
}

void Tracer::freeRayBuffers()
{
    for (auto& buf : bRay) {
        buf.reset();
    }
    for (auto& buf : bRayPayload) {
        buf.reset();
    }
    bTraceResult.reset();
}

void Tracer::freeAll()
{
    freeRayBuffers();
    bRayMetadata.reset();
    bScheduler.reset();
    bStats.reset();
    bStaging.reset();
}
}
