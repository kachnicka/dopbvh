#pragma once

#include "../../../Config.h"
#include "../../../Stats.h"
#include "../../VCtx.h"
#include "Types.h"
#include <unordered_map>
#include <vLime/Compute.h>
#include <vLime/Timestamp.h>
#include <vLime/vLime.h>

namespace backend::vulkan::data {
struct Scene;
}

namespace backend::vulkan::bvh {

struct PLOCpp {
    explicit PLOCpp(VCtx ctx);

    [[nodiscard]] Bvh GetBVH() const;
    [[nodiscard]] bool NeedsRecompute(config::PLOC const& buildConfig)
    {
        auto const cfgChanged { config != buildConfig };
        config = buildConfig;
        return cfgChanged && config.bv != config::BV::eNone;
    }

    void Compute(vk::CommandBuffer commandBuffer, data::Scene const& scene);
    void ReadRuntimeData();
    [[nodiscard]] stats::PLOC GatherStats(BvhStats const& bvhStats);

private:
    VCtx ctx;
    config::PLOC config;

    lime::PipelineCompute pInitialClusters;
    lime::PipelineCompute pCopySortedClusterIDs;
    lime::PipelineCompute pPLOCppIterations;

    struct Metadata {
        u32 nodeCountLeaf { 0 };
        u32 nodeCountTotal { 0 };

        u32 iterationCount { 0 };
        u32 workgroupSize { 0 };
        u32 workgroupSizePLOCpp { 0 };
    } metadata;

    enum class Buffer {
        eBVH,
        eBVHTriangles,
        eBVHTriangleIDs,

        eRadixEven,
        eRadixOdd,
        eRadixInternal,

        eRuntimeData,
        eDecoupledLookBack,

        eDebug,
    };
    lime::Buffer stagingBuffer;
    std::unordered_map<Buffer, lime::Buffer> buffersOut;
    std::unordered_map<Buffer, lime::Buffer> buffersIntermediate;
    vk::DeviceAddress nodeBuffer0Address { 0 };
    vk::DeviceAddress nodeBuffer1Address { 0 };

    void reloadPipelines();
    void alloc();
    void freeIntermediate();
    void freeAll();

    void initialClusters(vk::CommandBuffer commandBuffer, data::Scene const& scene);
    void sortClusterIDs(vk::CommandBuffer commandBuffer);
    void copySortedClusterIDs(vk::CommandBuffer commandBuffer);
    void iterationsSingleKernel(vk::CommandBuffer commandBuffer);

    struct Times {
        enum class Stamp : uint32_t {
            eInitialClustersAndWoopify,
            eSortClusterIDs,
            eCopySortedClusterIDs,
            ePLOCppIterations,
            eCount,
        };

        [[nodiscard]] inline static std::string to_string(Stamp name)
        {
            switch (name) {
            case Stamp::eInitialClustersAndWoopify:
                return "initial clusters and woopify";
            case Stamp::eSortClusterIDs:
                return "sort cluster IDs";
            case Stamp::eCopySortedClusterIDs:
                return "copy sorted cluster IDs";
            case Stamp::ePLOCppIterations:
                return "PLOCpp iterations";
            default:
                return "n/a";
            }
        }
        [[nodiscard]] inline static std::string to_string(u32 value)
        {
            return to_string(Stamp(value));
        }
    };
    lime::Timestamps<Times::Stamp> timestamps;
};

}
