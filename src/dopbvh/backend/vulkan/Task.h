#pragma once

#include <vLime/Queues.h>
#include <vLime/RenderGraph.h>

#include "VCtx.h"
#include "data/AccelerationStructure.h"
#include "data/ImGuiScene.h"
#include "data/Scene.h"

#include "workload/AabbDebugView.h"
#include "workload/ImGui.h"

#include "workload/bvh/Collapsing.h"
#include "workload/bvh/Compression.h"
#include "workload/bvh/PLOCpp.h"
#include "workload/bvh/Stats.h"
#include "workload/bvh/Tracer.h"
#include "workload/bvh/Transformation.h"

#include "workload/RayTracingKHR.h"

namespace backend::vulkan {
class Vulkan;
}

namespace backend::vulkan::task {

class PathTracerKHR {
private:
    friend class backend::vulkan::Vulkan;
    VCtx ctx;
    lime::rg::Graph rg;

public:
    PathTracingKHR renderer;

private:
    data::AccelerationStructure as_TMP;

    lime::rg::id::RayTracing ptTask;
    lime::rg::id::Resource ptImg;

public:
    explicit PathTracerKHR(VCtx ctx, lime::Queue queue)
        : ctx(ctx)
        , rg(ctx.d, ctx.memory)
        , renderer(ctx)
        , as_TMP(ctx, queue)
    {
    }

public:
    bool rebuildAS_TMP { false };
    bool mergeGeometryToOneBLAS { false };
    PathTracingKHR::Samples_TMP ptSamples_TMP;

public:
    struct RenderGraphIO {
        lime::rg::id::Resource ptImg;
    };

    void RecompileRenderGraph(u32 x, u32 y, RenderGraphIO const& rgIO = {})
    {
        rg.reset();

        // transition the image to general layout
        auto const transitionToGeneralTask { rg.AddTask<lime::rg::NoShader>() };
        rg.GetTask(transitionToGeneralTask).RegisterExecutionCallback([this](vk::CommandBuffer commandBuffer) {
            auto const currentLayout { rg.GetManagedBackingResource(ptImg).layout };
            auto const finalLayout { vk::ImageLayout::eGeneral };
            auto const accessFlags = lime::img_util::DefaultAccessFlags(currentLayout, finalLayout);
            vk::ImageMemoryBarrier memoryBarrier {
                .srcAccessMask = accessFlags.first,
                .dstAccessMask = accessFlags.second,
                .oldLayout = currentLayout,
                .newLayout = finalLayout,
                .image = rg.GetResource(ptImg).physicalResourceDetail.back().image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                {}, nullptr, nullptr, memoryBarrier);
            rg.GetManagedBackingResource(ptImg).layout = finalLayout;
        });

        if (rgIO.ptImg.isValid())
            ptImg = rgIO.ptImg;
        else
            ptImg = rg.AddResource(vk::Format::eR32G32B32A32Sfloat);
        ptTask = rg.AddTask<lime::rg::RayTracing>();

        rg.SetTaskResourceOperation(ptTask, ptImg, lime::rg::Resource::OperationFlagBits::eStored);
        // TODO: this achieves correct flag, but is logically wrong, because this resource is sampled in another task_old_style (in another RG)
        rg.SetTaskResourceOperation(ptTask, ptImg, lime::rg::Resource::OperationFlagBits::eSampledFrom);

        rg.GetResource(ptImg).extent = vk::Extent3D { x, y, 1 };
        rg.GetResource(ptImg).finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        rg.GetTask(ptTask).RegisterExecutionCallback([this](vk::CommandBuffer commandBuffer) {
            renderer.recordCommands(commandBuffer, rg.GetResource(ptImg).extent.width, rg.GetResource(ptImg).extent.height, ptSamples_TMP);
        });

        // transition to final layout
        auto const transitionToFinalTask { rg.AddTask<lime::rg::NoShader>() };
        rg.GetTask(transitionToFinalTask).RegisterExecutionCallback([this](vk::CommandBuffer commandBuffer) {
            auto const currentLayout { vk::ImageLayout::eGeneral };
            auto const finalLayout { rg.GetResource(ptImg).finalLayout };
            auto const accessFlags = lime::img_util::DefaultAccessFlags(currentLayout, finalLayout);
            vk::ImageMemoryBarrier memoryBarrier {
                .srcAccessMask = accessFlags.first,
                .dstAccessMask = accessFlags.second,
                .oldLayout = currentLayout,
                .newLayout = finalLayout,
                .image = rg.GetResource(ptImg).physicalResourceDetail.back().image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eRayTracingShaderKHR,
                vk::PipelineStageFlagBits::eFragmentShader,
                {}, nullptr, nullptr, memoryBarrier);
            rg.GetManagedBackingResource(ptImg).layout = finalLayout;
        });

        rg.Compile();
    }

    void RebuildAS()
    {
        rebuildAS_TMP = true;
    }

    lime::Image& GetTarget_TMP()
    {
        return rg.GetManagedBackingResource(ptImg);
    }

    lime::Buffer::Detail GetAtomicCounter_TMP() const
    {
        return static_cast<lime::Buffer::Detail>(renderer.rayAtomicCounter);
    }

    f32 ReadTimestamps_TMP()
    {
        renderer.times.total = renderer.times.stamps.GetResultsNs();
        renderer.times.computeSum();
        return renderer.times.sum;
    }

    void RenderFrame(vk::CommandBuffer commandBuffer, data::DeviceData const& data_TMP, data::Scene const& scene)
    {
        if (ptSamples_TMP.toCompute == 0)
            return;

        if (rebuildAS_TMP) {
            as_TMP.Build(data_TMP, scene, mergeGeometryToOneBLAS);
            rebuildAS_TMP = false;
        }
        renderer.updateDSet(rg.GetResource(ptImg).physicalResourceDetail[0].imageView, as_TMP.getTLAS(), as_TMP.getSceneDescription(), data_TMP.cameraBuffer);
        rg.SetupExecution(commandBuffer);
    }
};

class PathTracerCompute {
    friend class backend::vulkan::Vulkan;
    VCtx ctx;
    lime::Queue queue;

    lime::rg::Graph rg;
    lime::rg::id::Compute ptTask;
    lime::rg::id::Resource ptImg;

    bvh::PLOCpp plocpp;
    bvh::Collapsing collapsing;
    bvh::Transformation transformation;

public:
    bvh::Compression compression;

    bvh::Stats stats;
    bvh::Tracer tracer;

private:
    bvh::TraceRuntime traceRuntimeData;

public:
    explicit PathTracerCompute(VCtx ctx, lime::Queue queue)
        : ctx(ctx)
        , queue(queue)
        , rg(ctx.d, ctx.memory)
        , plocpp(ctx)
        , collapsing(ctx)
        , transformation(ctx)
        , compression(ctx)
        , stats(ctx)
        , tracer(ctx)
    {
    }

    struct RenderGraphIO {
        lime::rg::id::Resource ptImg;
    };

    void TTI_TMP()
    {
        if (tracer.readTs) {
            f32 t { tracer.timestamp.ReadTimeNs() * 1e-6f };
            std::cout << t << "\n"; // fmt::format("{:2.f}\n", t);
            tracer.readTs = false;
        }
    }

    void RecompileRenderGraph(u32 x, u32 y, RenderGraphIO const& rgIO = {})
    {
        rg.reset();

        // transition the image to general layout
        auto const transitionToGeneralTask { rg.AddTask<lime::rg::NoShader>() };
        rg.GetTask(transitionToGeneralTask).RegisterExecutionCallback([this](vk::CommandBuffer commandBuffer) {
            auto const currentLayout { rg.GetManagedBackingResource(ptImg).layout };
            auto const finalLayout { vk::ImageLayout::eGeneral };
            auto const accessFlags = lime::img_util::DefaultAccessFlags(currentLayout, finalLayout);
            vk::ImageMemoryBarrier memoryBarrier {
                .srcAccessMask = accessFlags.first,
                .dstAccessMask = accessFlags.second,
                .oldLayout = currentLayout,
                .newLayout = finalLayout,
                .image = rg.GetResource(ptImg).physicalResourceDetail.back().image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eFragmentShader,
                vk::PipelineStageFlagBits::eComputeShader,
                {}, nullptr, nullptr, memoryBarrier);
            rg.GetManagedBackingResource(ptImg).layout = finalLayout;
        });

        if (rgIO.ptImg.isValid())
            ptImg = rgIO.ptImg;
        else
            ptImg = rg.AddResource(vk::Format::eR32G32B32A32Sfloat);
        ptTask = rg.AddTask<lime::rg::Compute>();

        rg.SetTaskResourceOperation(ptTask, ptImg, lime::rg::Resource::OperationFlagBits::eStored);
        // TODO: this achieves correct flag, but is logically wrong, because this resource is sampled in another task (in another RG)
        rg.SetTaskResourceOperation(ptTask, ptImg, lime::rg::Resource::OperationFlagBits::eSampledFrom);

        rg.GetResource(ptImg).extent = vk::Extent3D { x, y, 1 };
        rg.GetResource(ptImg).finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;

        rg.GetTask(ptTask).RegisterExecutionCallback([this](vk::CommandBuffer commandBuffer) {
            if (buildState != BuildState::eDone)
                return;
            if (buildConfig.tracer.bv == config::BV::eNone)
                return;
            traceRuntimeData.x = rg.GetResource(ptImg).extent.width;
            traceRuntimeData.y = rg.GetResource(ptImg).extent.height;
            tracer.Trace(commandBuffer, buildConfig.tracer, traceRuntimeData, compression.GetBVH());
        });

        // transition to final layout
        auto const transitionToFinalTask { rg.AddTask<lime::rg::NoShader>() };
        rg.GetTask(transitionToFinalTask).RegisterExecutionCallback([this](vk::CommandBuffer commandBuffer) {
            auto const currentLayout { vk::ImageLayout::eGeneral };
            auto const finalLayout { rg.GetResource(ptImg).finalLayout };
            auto const accessFlags = lime::img_util::DefaultAccessFlags(currentLayout, finalLayout);
            vk::ImageMemoryBarrier memoryBarrier {
                .srcAccessMask = accessFlags.first,
                .dstAccessMask = accessFlags.second,
                .oldLayout = currentLayout,
                .newLayout = finalLayout,
                .image = rg.GetResource(ptImg).physicalResourceDetail.back().image,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            commandBuffer.pipelineBarrier(
                vk::PipelineStageFlagBits::eComputeShader,
                vk::PipelineStageFlagBits::eFragmentShader,
                {}, nullptr, nullptr, memoryBarrier);
            rg.GetManagedBackingResource(ptImg).layout = finalLayout;
        });
        rg.Compile();
    }

    lime::Image& GetTarget_TMP()
    {
        return rg.GetManagedBackingResource(ptImg);
    }

    stats::BVHPipeline const& GetStatsBuild() const
    {
        return statsBuild;
    }

    stats::Trace const& GetStatsTrace() const
    {
        return statsTrace;
    }

    void RenderFrame(vk::CommandBuffer commandBuffer, data::DeviceData const& data_TMP, data::Scene const& scene)
    {
        if (traceRuntimeData.samples.toCompute == 0)
            return;

        static_cast<void>(scene);
        traceRuntimeData.targetImageView = rg.GetResource(ptImg).physicalResourceDetail[0].imageView;
        traceRuntimeData.camera = data_TMP.cameraBuffer;

        statsTrace = tracer.GetStats();

        rg.SetupExecution(commandBuffer);
    }

    config::BVHPipeline GetConfig() const
    {
        return buildConfig;
    }

    void SetPipelineConfiguration(config::BVHPipeline config)
    {
        buildConfig = std::move(config);

        if (compression.NeedsRecompute(buildConfig.compression))
            buildState = BuildState::eCompression;
        if (transformation.NeedsRecompute(buildConfig.transformation))
            buildState = BuildState::eTransformation;
        if (collapsing.NeedsRecompute(buildConfig.collapsing))
            buildState = BuildState::eCollapsing;
        if (plocpp.NeedsRecompute(buildConfig.plocpp))
            buildState = BuildState::ePLOC;
    }

    bool BVHBuildPiecewise(data::Scene const& scene)
    {
        if (buildState == BuildState::eDone)
            return false;

        stats.SetSceneAabbSurfaceArea(scene.aabb.Area());

        traceRuntimeData.geometryDescriptorAddress = scene.data->sceneDescriptionBuffer.getDeviceAddress(ctx.d);
        lime::commands::TransientPool transientPool { ctx.d, queue };
        auto fence { lime::FenceFactory(ctx.d) };
        vk::CommandBuffer commandBuffer;

        berry::Log::debug("BVH build: {}", buildConfig.name);
        while (buildState != BuildState::eDone) {
            switch (buildState) {
            case BuildState::ePLOC:
                berry::Log::debug("BVH build stage: PLOCpp");
                commandBuffer = transientPool.BeginCommands();
                plocpp.Compute(commandBuffer, scene);
                transientPool.EndSubmitCommands(commandBuffer, fence.get());
                plocpp.ReadRuntimeData();

                berry::Log::debug("BVH build stage: PLOCpp stats");
                commandBuffer = transientPool.BeginCommands();
                buildConfig.stats.bv = buildConfig.plocpp.bv;
                stats.Compute(commandBuffer, buildConfig.stats, plocpp.GetBVH());
                transientPool.EndSubmitCommands(commandBuffer, fence.get());

                statsBuild.plocpp = plocpp.GatherStats(*stats.data);
                buildState = BuildState::eCollapsing;
                break;
            case BuildState::eCollapsing:
                if (buildConfig.collapsing.bv != config::BV::eNone) {
                    berry::Log::debug("BVH build stage: Collapsing");
                    commandBuffer = transientPool.BeginCommands();
                    collapsing.Compute(commandBuffer, plocpp.GetBVH(), traceRuntimeData.geometryDescriptorAddress);
                    transientPool.EndSubmitCommands(commandBuffer, fence.get());
                    collapsing.ReadRuntimeData();

                    berry::Log::debug("BVH build stage: Collapsing stats");
                    commandBuffer = transientPool.BeginCommands();
                    buildConfig.stats.bv = buildConfig.collapsing.bv;
                    stats.Compute(commandBuffer, buildConfig.stats, collapsing.GetBVH());
                    transientPool.EndSubmitCommands(commandBuffer, fence.get());

                    statsBuild.collapsing = collapsing.GatherStats(*stats.data);
                }
                buildState = buildConfig.transformation.bv == config::BV::eNone ? BuildState::eCompression : BuildState::eTransformation;
                break;
            case BuildState::eTransformation:
                if (buildConfig.transformation.bv != config::BV::eNone) {
                    berry::Log::debug("BVH build stage: Transformation");
                    commandBuffer = transientPool.BeginCommands();
                    transformation.Compute(commandBuffer, collapsing.GetBVH(), traceRuntimeData.geometryDescriptorAddress);
                    transientPool.EndSubmitCommands(commandBuffer, fence.get());
                    transformation.ReadRuntimeData();

                    berry::Log::debug("BVH build stage: Transformation stats");
                    commandBuffer = transientPool.BeginCommands();
                    buildConfig.stats.bv = buildConfig.transformation.bv;
                    stats.Compute(commandBuffer, buildConfig.stats, transformation.GetBVH());
                    transientPool.EndSubmitCommands(commandBuffer, fence.get());

                    statsBuild.transformation = transformation.GatherStats(*stats.data);
                }
                buildState = BuildState::eCompression;
                break;
            case BuildState::eCompression:
                if (buildConfig.compression.bv != config::BV::eNone) {
                    berry::Log::debug("BVH build stage: Compression");
                    commandBuffer = transientPool.BeginCommands();
                    compression.Compute(commandBuffer, (buildConfig.transformation.bv == config::BV::eNone ? collapsing.GetBVH() : transformation.GetBVH()));
                    transientPool.EndSubmitCommands(commandBuffer, fence.get());
                    compression.ReadRuntimeData();

                    berry::Log::debug("BVH build stage: Compression stats");
                    commandBuffer = transientPool.BeginCommands();
                    buildConfig.stats.bv = buildConfig.compression.bv;
                    stats.Compute(commandBuffer, buildConfig.stats, compression.GetBVH());
                    transientPool.EndSubmitCommands(commandBuffer, fence.get());

                    statsBuild.compression = compression.GatherStats(*stats.data);
                }
                buildState = BuildState::eDone;
                break;
            case BuildState::eDone:
                break;
            }
        }
        if (buildConfig.transformation.bv == config::BV::eNone)
            statsBuild.transformation = {};
        // statsBuild.print();
        berry::Log::debug("BVH build done.");
        return true;
    }

private:
    enum class BuildState {
        eDone,
        ePLOC,
        eCollapsing,
        eTransformation,
        eCompression,
    } buildState { BuildState::ePLOC };
    config::BVHPipeline buildConfig;

    stats::BVHPipeline statsBuild;
    stats::Trace statsTrace;
};

class DebugView {
private:
    friend class backend::vulkan::Vulkan;
    VCtx ctx;

    vk::Format depthFormat;
    lime::rg::Graph rg;
    lime::rg::id::Rasterization taskStencil;
    lime::rg::id::Rasterization taskHeatWireframe;
    lime::rg::id::Resource imgStencil;
    lime::rg::id::Resource imgHeatWireframe;
    lime::rg::id::Resource imgHeatWireframeDepth;

public:
    AabbRenderer_Stencil stencil;
    AabbRenderer_HeatWireframe heatWireframe;

private:
    data::Scene* sceneToRender { nullptr };

public:
    DebugView(VCtx ctx, vk::Format depthFormat)
        : ctx(ctx)
        , depthFormat(depthFormat)
        , rg(ctx.d, ctx.memory)
        , stencil(ctx.d, ctx.sCache)
        , heatWireframe(ctx.d, ctx.sCache)
    {
    }

    void RecompileRenderGraph(u32 x, u32 y)
    {
        rg.reset();

        taskStencil = rg.AddTask<lime::rg::Rasterization>();
        imgStencil = rg.AddResource(vk::Format::eS8Uint);
        rg.SetTaskResourceOperation(taskStencil, imgStencil, lime::rg::Resource::OperationFlagBits::eAttachment);
        rg.GetResource(imgStencil).finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        rg.GetResource(imgStencil).extent = vk::Extent3D { x, y, 1 };

        rg.GetTask(taskStencil).RegisterExecutionCallback([this](vk::CommandBuffer commandBuffer, lime::rg::RasterizationInfo const& info) {
            stencil.recordCommands(commandBuffer, info, *sceneToRender);
        });

        taskHeatWireframe = rg.AddTask<lime::rg::Rasterization>();
        rg.GetTask(taskHeatWireframe).task.msaa.sampleCount = vk::SampleCountFlagBits::e4;

        imgHeatWireframe = rg.AddResource(vk::Format::eR8G8B8A8Unorm);
        rg.GetResource(imgHeatWireframe).finalLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        rg.GetResource(imgHeatWireframe).inheritExtentFromResource = imgStencil;
        rg.SetTaskResourceOperation(taskHeatWireframe, imgStencil, lime::rg::Resource::OperationFlagBits::eSampledFrom);
        rg.SetTaskResourceOperation(taskHeatWireframe, imgHeatWireframe, lime::rg::Resource::OperationFlagBits::eAttachment);

        rg.SetTaskResourceOperation(taskHeatWireframe, imgHeatWireframe, lime::rg::Resource::OperationFlagBits::eSampledFrom);

        imgHeatWireframeDepth = rg.AddResource(depthFormat);
        rg.GetResource(imgHeatWireframeDepth).finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
        rg.GetResource(imgHeatWireframeDepth).inheritExtentFromResource = imgStencil;
        rg.SetTaskResourceOperation(taskHeatWireframe, imgHeatWireframeDepth, lime::rg::Resource::OperationFlagBits::eAttachment);

        rg.GetTask(taskHeatWireframe).RegisterExecutionCallback([this](vk::CommandBuffer commandBuffer, lime::rg::RasterizationInfo const& info) {
            heatWireframe.recordCommands(commandBuffer, info, *sceneToRender);
        });

        rg.Compile();
    }

    lime::Image& GetTarget_TMP()
    {
        return rg.GetManagedBackingResource(imgHeatWireframe);
    }

    f32& GetHeatMapScale_TMP()
    {
        return heatWireframe.heatMapScale;
    }

    void RenderFrame(vk::CommandBuffer commandBuffer, data::DeviceData const& data_TMP, data::Scene& scene)
    {
        sceneToRender = &scene;
        stencil.Describe(data_TMP.cameraBuffer, sceneToRender->aabbBuffer, sceneToRender->transformBuffer);
        heatWireframe.Describe(data_TMP.cameraBuffer, rg.GetManagedBackingResource(imgStencil).getView(), sceneToRender->aabbBuffer, sceneToRender->transformBuffer);
        rg.SetupExecution(commandBuffer);
    }
};

}

namespace backend::vulkan::task_old_style {

struct ImGui {
    lime::rg::id::Rasterization task;
    lime::rg::id::Resource imgRendered;
    lime::rg::id::Resource imgTarget;

    Renderer_imGUI renderer;
    data::Scene_imGUI scene;

    explicit ImGui(VCtx ctx)
        : renderer(ctx.d, ctx.sCache)
        , scene(ctx)
    {
    }

    void scheduleToRenderGraph(lime::rg::Graph& rg)
    {
        task = rg.AddTask<lime::rg::Rasterization>();
        rg.GetTask(task).RegisterExecutionCallback([this](vk::CommandBuffer commandBuffer, lime::rg::RasterizationInfo const& info) {
            renderer.recordCommands(commandBuffer, info, scene);
        });

        if (!imgRendered.isValid())
            imgRendered = rg.AddResource(vk::Format::eR8G8B8A8Unorm);
        rg.SetTaskResourceOperation(task, imgRendered, lime::rg::Resource::OperationFlagBits::eSampledFrom);

        if (!imgTarget.isValid())
            imgTarget = rg.AddResource(vk::Format::eR8G8B8A8Unorm);
        rg.SetTaskResourceOperation(task, imgTarget, lime::rg::Resource::OperationFlagBits::eStored);
    }
};

}
