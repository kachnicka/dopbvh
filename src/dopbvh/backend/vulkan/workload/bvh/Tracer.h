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

struct Tracer {
    enum class Mode {
        ePathTracing,
        eBVVisualization,
        eBVIntersections,
    } mode { Mode::ePathTracing };

    explicit Tracer(VCtx ctx);

    [[nodiscard]] config::Tracer const& GetConfig() const
    {
        return config;
    }

    void Trace(vk::CommandBuffer commandBuffer, config::Tracer const& traceCfg, TraceRuntime const& trt, Bvh const& inputBvh);
    [[nodiscard]] stats::Trace GetStats() const;

private:
    VCtx ctx;
    config::Tracer config;

    lime::PipelineCompute pTrace;
    lime::PipelineCompute pPrimaryRayGen;
    lime::PipelineCompute pShadeAndCast;

    struct {
        u32 workgroupSize { 0 };

        u32 rayCount { 0 };
        Bvh::Layout bvhMemoryLayout { Bvh::Layout::eBinaryCompressed };
        bool reloadPipelines { true };
    } metadata;

    enum class Buffer {
        eScheduler,
    };

    lime::Buffer bStaging;
    lime::Buffer bScheduler;
    lime::Buffer bStats;
    lime::Buffer bRayMetadata;

    lime::Buffer bRay[2];
    lime::Buffer bRayPayload[2];
    lime::Buffer bTraceResult;

    vk::UniqueDescriptorPool dPool;
    vk::DescriptorSet dSet;

    void reloadPipelines();
    void dSetUpdate(vk::ImageView targetImageView, lime::Buffer::Detail const& camera) const;
    void allocRayBuffers(uint32_t rayCount);
    void allocStatic();

    void trace_joined(vk::CommandBuffer commandBuffer, TraceRuntime const& trt, Bvh const& inputBvh);
    void trace_separate(vk::CommandBuffer commandBuffer, TraceRuntime const& trt, Bvh const& inputBvh, std::function<void(vk::CommandBuffer commandBuffer)> const& additionalCommands = {});

    void freeRayBuffers();
    void freeAll();

public:
    f32 dirLight_TMP[4];

    bool readTs { false };
    lime::SingleTimer timestamp;
};

}
