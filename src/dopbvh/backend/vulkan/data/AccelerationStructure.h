#pragma once

#include "../VCtx.h"
#include "DeviceData.h"
#include "Scene.h"
#include <berries/lib_helper/spdlog.h>
#include <vLime/CommandPool.h>
#include <vLime/Memory.h>
#include <vLime/Queues.h>
#include <vLime/Timestamp.h>
#include <vLime/Util.h>
#include <vLime/vLime.h>

namespace backend::vulkan::data {

class AccelerationStructure {
public:
    struct GeometryInstance {
        u64 vertexAddress { lime::INVALID_ID };
        u64 indexAddress { lime::INVALID_ID };
        u64 normalAddress { lime::INVALID_ID };
        u64 uvAddress { lime::INVALID_ID };
    };

private:
    VCtx ctx;
    lime::Queue queue;

    std::vector<vk::UniqueAccelerationStructureKHR> blas;
    vk::UniqueAccelerationStructureKHR tlas;
    lime::Buffer blasBuffer;
    lime::Buffer tlasBuffer;
    lime::Buffer sceneDescriptionBuffer;

    struct Times {
        enum class Stamp : u32 {
            eBLASBuild,
            eTLASBuild,
            eCount,
        };

        [[nodiscard]] inline static std::string to_string(Stamp name)
        {
            switch (name) {
            case Stamp::eBLASBuild:
                return "BLAS build";
            case Stamp::eTLASBuild:
                return "TLAS build";
            default:
                return "n/a";
            }
        }

        Times(vk::Device d, vk::PhysicalDevice pd)
            : stamps(d, pd)
        {
        }

        lime::Timestamps<Times::Stamp> stamps;

        std::vector<f32> times;
    } times;

public:
    AccelerationStructure(VCtx ctx, lime::Queue queue)
        : ctx(ctx)
        , queue(queue)
        , times(ctx.d, ctx.pd)
    {
    }

    [[nodiscard]] vk::AccelerationStructureKHR getTLAS() const
    {
        return tlas.get();
    }

    [[nodiscard]] lime::Buffer::Detail getSceneDescription() const
    {
        return sceneDescriptionBuffer;
    }

    void Build(DeviceData const& data_TMP, Scene const& scene, bool allGeometryToOneBLAS = false)
    {
        lime::check(ctx.d.waitIdle());
        blas.clear();
        tlas.reset();

        lime::commands::TransientPool transientPool { ctx.d, queue };

        std::vector<GeometryInstance> geometryInstances;
        std::vector<vk::AccelerationStructureGeometryKHR> asGeometries;
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR> buildRanges;
        std::vector<vk::AccelerationStructureBuildGeometryInfoKHR> buildInfos;
        std::vector<vk::AccelerationStructureBuildSizesInfoKHR> buildSizes;

        std::vector<vk::DeviceSize> scratchSizes;
        std::vector<vk::AccelerationStructureBuildRangeInfoKHR*> buildRangePtrs;

        geometryInstances.reserve(scene.geometries.size());
        asGeometries.reserve(scene.geometries.size());
        buildRanges.reserve(scene.geometries.size());
        buildInfos.reserve(scene.geometries.size());
        buildSizes.reserve(scene.geometries.size());
        scratchSizes.reserve(scene.geometries.size());
        buildRangePtrs.reserve(scene.geometries.size());
        blas.reserve(scene.geometries.size());

        auto const blasUsage { vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress };
        auto const tlasUsage { vk::BufferUsageFlagBits::eAccelerationStructureStorageKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress };

        vk::DeviceSize scratchSize { 0 };
        vk::DeviceSize blasSize { 0 };
        vk::DeviceSize blasAlignment { ctx.memory.dummyBufferRequirements(blasUsage).alignment };

        u32 blasCounter { 0 };

        std::vector<u32> maxPrimitiveCounts;
        maxPrimitiveCounts.reserve(scene.geometries.size());

        for (auto const& gId : scene.geometries) {
            auto& g { data_TMP.geometries[gId] };

            asGeometries.emplace_back();
            auto& asGeometry { asGeometries.back() };
            asGeometry.geometry.triangles = vk::AccelerationStructureGeometryTrianglesDataKHR {};
            asGeometry.flags = vk::GeometryFlagBitsKHR::eOpaque; // | vk::GeometryFlagBitsKHR::eNoDuplicateAnyHitInvocation;
            asGeometry.geometryType = vk::GeometryTypeKHR::eTriangles;
            asGeometry.geometry.triangles.vertexData.deviceAddress = ctx.d.getBufferAddress({ .buffer = g.vertexBuffer.resource }) + g.vertexBuffer.offset;
            asGeometry.geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;
            asGeometry.geometry.triangles.vertexStride = 3 * sizeof(f32);
            asGeometry.geometry.triangles.indexData.deviceAddress = ctx.d.getBufferAddress({ .buffer = g.indexBuffer.resource }) + g.indexBuffer.offset;
            asGeometry.geometry.triangles.indexType = vk::IndexType::eUint32;
            asGeometry.geometry.triangles.maxVertex = g.vertexCount;

            buildRanges.emplace_back();
            auto& buildRange { buildRanges.back() };
            buildRange.primitiveCount = g.indexCount / 3;
            buildRange.primitiveOffset = 0;
            buildRange.firstVertex = 0;
            buildRange.transformOffset = 0;

            geometryInstances.push_back({
                getBufferAddress(g.vertexBuffer),
                getBufferAddress(g.indexBuffer),
                getBufferAddress(g.normalBuffer),
                getBufferAddress(g.uvBuffer),
            });

            if (!allGeometryToOneBLAS) {
                buildInfos.emplace_back();
                auto& buildInfo { buildInfos.back() };
                buildInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
                buildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
                buildInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
                buildInfo.geometryCount = 1;
                buildInfo.pGeometries = &asGeometry;

                buildSizes.push_back(ctx.d.getAccelerationStructureBuildSizesKHR(
                    vk::AccelerationStructureBuildTypeKHR::eDevice,
                    buildInfo, { buildRange.primitiveCount }));

                scratchSize = std::max(scratchSize, buildSizes.back().buildScratchSize);
                blasSize = lime::memory::align(blasSize, blasAlignment) + buildSizes.back().accelerationStructureSize;
                blasCounter++;

                scratchSizes.emplace_back(lime::memory::align(buildSizes.back().buildScratchSize, blasAlignment));
            } else
                maxPrimitiveCounts.emplace_back(buildRange.primitiveCount);
        }

        if (allGeometryToOneBLAS) {
            buildInfos.emplace_back();
            auto& buildInfo { buildInfos.back() };
            buildInfo.type = vk::AccelerationStructureTypeKHR::eBottomLevel;
            buildInfo.mode = vk::BuildAccelerationStructureModeKHR::eBuild;
            buildInfo.flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace;
            buildInfo.geometryCount = static_cast<u32>(asGeometries.size());
            buildInfo.pGeometries = asGeometries.data();

            buildSizes.push_back(ctx.d.getAccelerationStructureBuildSizesKHR(
                vk::AccelerationStructureBuildTypeKHR::eDevice,
                buildInfo, maxPrimitiveCounts));

            scratchSize = std::max(scratchSize, buildSizes.back().buildScratchSize);
            blasSize = lime::memory::align(blasSize, blasAlignment) + buildSizes.back().accelerationStructureSize;
            blasCounter++;
            scratchSizes.emplace_back(lime::memory::align(buildSizes.back().buildScratchSize, blasAlignment));
        }

        auto const sceneDescriptionSize { geometryInstances.size() * sizeof(GeometryInstance) };
        sceneDescriptionBuffer.reset();
        sceneDescriptionBuffer = ctx.memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal },
            {
                .size = sceneDescriptionSize,
                .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
            },
            "rt_scene_description");
        ctx.transfer.ToDeviceSync(geometryInstances, sceneDescriptionBuffer);

        auto const asProperties { ctx.pd.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceAccelerationStructurePropertiesKHR>().get<vk::PhysicalDeviceAccelerationStructurePropertiesKHR>() };
        auto const scratchTotalSize { std::accumulate(scratchSizes.begin(), scratchSizes.end(), 0ull) };
        auto scratchBuffer { ctx.memory.alloc(
            {
                .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal,
                .additionalAlignment = asProperties.minAccelerationStructureScratchOffsetAlignment,
            },
            {
                .size = scratchTotalSize,
                .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            },
            "rt_scratch") };
        auto scratchAddress { ctx.d.getBufferAddress({ .buffer = scratchBuffer.get() }) };

        blasBuffer.reset();
        blasBuffer = ctx.memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal },
            {
                .size = blasSize,
                .usage = blasUsage,
            },
            "rt_blas");
        lime::LinearAllocator blasAllocator { blasBuffer };

        auto commandBuffer { transientPool.BeginCommands() };
        times.stamps.Reset(commandBuffer);
        times.stamps.WriteBeginStamp(commandBuffer);

        lime::debug::BeginDebugLabel(commandBuffer, "BLAS build", lime::debug::LabelColor::ePurple);

        for (u32 i { 0 }; i < blasCounter; i++) {
            auto const subAllocation { blasAllocator.alloc(buildSizes[i].accelerationStructureSize, blasAlignment) };
            vk::AccelerationStructureCreateInfoKHR const cInfo {
                .buffer = subAllocation.resource,
                .offset = subAllocation.offset,
                .size = subAllocation.size,
                .type = vk::AccelerationStructureTypeKHR::eBottomLevel,
            };
            blas.push_back(lime::check(ctx.d.createAccelerationStructureKHRUnique(cInfo)));
            lime::debug::SetObjectName(blas.back().get(), std::format("blas_{}", i).c_str(), ctx.d);
            buildRangePtrs.emplace_back(&buildRanges[i]);

            if (i > 0)
                scratchAddress += scratchSizes[i - 1];
            buildInfos[i].scratchData.deviceAddress = scratchAddress;
            buildInfos[i].dstAccelerationStructure = blas.back().get();
        }
        commandBuffer.buildAccelerationStructuresKHR(blasCounter, buildInfos.data(), buildRangePtrs.data());

        // finish BLASes build before TLAS build
        vk::MemoryBarrier const memoryBarrier {
            .srcAccessMask = vk::AccessFlagBits::eAccelerationStructureWriteKHR,
            .dstAccessMask = vk::AccessFlagBits::eAccelerationStructureReadKHR,
        };
        commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR, vk::DependencyFlags(), memoryBarrier, nullptr, nullptr);
        lime::debug::EndDebugLabel(commandBuffer);
        times.stamps.Write(commandBuffer, Times::Stamp::eBLASBuild, 0, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR);

        // TLAS
        std::vector<vk::AccelerationStructureInstanceKHR> tlasInstances;
        tlasInstances.reserve(blas.size());

        blasCounter = 0;
        vk::AccelerationStructureDeviceAddressInfoKHR addressInfo;

        if (allGeometryToOneBLAS) {
            tlasInstances.emplace_back();
            auto& instance { tlasInstances.back() };
            // memcpy(static_cast<void*>(&instance.transform), lime::IDENTITY_MATRIX.data(), sizeof(instance.transform));
            memcpy(static_cast<void*>(&instance.transform), scene.toWorld[0].data(), sizeof(instance.transform));
            instance.instanceCustomIndex = blasCounter++;
            instance.mask = 0xff;
            instance.instanceShaderBindingTableRecordOffset = 0;
            instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
            addressInfo.accelerationStructure = blas[0].get();
            auto blasAddress { ctx.d.getAccelerationStructureAddressKHR(&addressInfo) };
            instance.accelerationStructureReference = blasAddress;
        } else
            for (u32 i { 0 }; i < scene.nodeGeometry.size(); i++) {
                tlasInstances.emplace_back();
                auto& instance { tlasInstances.back() };
                // memcpy(static_cast<void*>(&instance.transform), lime::IDENTITY_MATRIX.data(), sizeof(instance.transform));
                memcpy(static_cast<void*>(&instance.transform), scene.toWorld[i].data(), sizeof(instance.transform));
                instance.instanceCustomIndex = blasCounter++;
                // The visibility mask is always set of 0xFF, but if some instances would need to be ignored in
                // some cases, this flag should be passed by the application
                instance.mask = 0xff;
                // Set the hit group index, that will be used to find the shader code to execute when hitting
                // the geometry
                instance.instanceShaderBindingTableRecordOffset = 0;
                // Disable culling - more fine control could be provided by the application
                instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
                //            instance.flags = vk::GeometryInstanceFlagBitsKHR::eTriangleFacingCullDisable;

                addressInfo.accelerationStructure = blas[i].get();
                auto blasAddress { ctx.d.getAccelerationStructureAddressKHR(&addressInfo) };
                instance.accelerationStructureReference = blasAddress;
            }

        auto const instancesSize { tlasInstances.size() * sizeof(vk::AccelerationStructureInstanceKHR) };
        auto instancesBuffer { ctx.memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal },
            {
                .size = instancesSize,
                .usage = vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
            },
            "rt_instances_buffer") };
        ctx.transfer.ToDeviceSync(tlasInstances, instancesBuffer);

        vk::AccelerationStructureGeometryInstancesDataKHR instancesVk {
            .arrayOfPointers = vk::False,
            .data = { .deviceAddress = ctx.d.getBufferAddress({ .buffer = instancesBuffer.get() }) },
        };

        vk::AccelerationStructureGeometryKHR topASGeometry {
            .geometryType = vk::GeometryTypeKHR::eInstances,
            .geometry {
                .instances = instancesVk,
            },
        };

        // query sizes
        vk::AccelerationStructureBuildGeometryInfoKHR buildInfo {
            .type = vk::AccelerationStructureTypeKHR::eTopLevel,
            .flags = vk::BuildAccelerationStructureFlagBitsKHR::ePreferFastTrace,
            .mode = vk::BuildAccelerationStructureModeKHR::eBuild,
            // .mode = update ? vk::BuildAccelerationStructureModeKHR::eUpdate : vk::BuildAccelerationStructureModeKHR::eBuild,
            .srcAccelerationStructure = nullptr,
            .geometryCount = 1,
            .pGeometries = &topASGeometry,
        };

        auto const instanceCount { static_cast<u32>(tlasInstances.size()) };
        vk::AccelerationStructureBuildSizesInfoKHR sizeInfo;
        ctx.d.getAccelerationStructureBuildSizesKHR(vk::AccelerationStructureBuildTypeKHR::eDevice, &buildInfo, &instanceCount, &sizeInfo);

        tlasBuffer.reset();
        tlasBuffer = ctx.memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal },
            {
                .size = sizeInfo.accelerationStructureSize,
                .usage = tlasUsage,
            },
            "rt_tlas");

        vk::AccelerationStructureCreateInfoKHR const cInfo {
            .buffer = tlasBuffer.get(),
            .offset = 0,
            .size = tlasBuffer.getSizeInBytes(),
            .type = vk::AccelerationStructureTypeKHR::eTopLevel,
        };
        tlas = lime::check(ctx.d.createAccelerationStructureKHRUnique(cInfo));
        lime::debug::SetObjectName(tlas.get(), "tlas", ctx.d);

        // allocRayBuffers scratch for tlas if necessary
        lime::Buffer tlasScratchBuffer;
        if (sizeInfo.buildScratchSize > scratchSize) {
            tlasScratchBuffer = ctx.memory.alloc(
                {
                    .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal,
                    .additionalAlignment = asProperties.minAccelerationStructureScratchOffsetAlignment,
                },
                {
                    .size = sizeInfo.buildScratchSize,
                    .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
                },
                "rt_tlas_scratch");
        }
        auto scratchBufferForTlasBuild { tlasScratchBuffer.get() ? tlasScratchBuffer.get() : scratchBuffer.get() };

        buildInfo.srcAccelerationStructure = nullptr;
        //            buildInfo.srcAccelerationStructure = update ? tlas.as.get() : VK_NULL_HANDLE;
        buildInfo.dstAccelerationStructure = tlas.get();
        buildInfo.scratchData.deviceAddress = ctx.d.getBufferAddress({ .buffer = scratchBufferForTlasBuild });

        vk::AccelerationStructureBuildRangeInfoKHR buildOffsetInfo { instanceCount, 0, 0, 0 };
        vk::AccelerationStructureBuildRangeInfoKHR const* pBuildOffsetInfo = &buildOffsetInfo;

        lime::debug::BeginDebugLabel(commandBuffer, "TLAS build", lime::debug::LabelColor::ePurple);
        commandBuffer.buildAccelerationStructuresKHR(1, &buildInfo, &pBuildOffsetInfo);
        lime::debug::EndDebugLabel(commandBuffer);
        times.stamps.Write(commandBuffer, Times::Stamp::eTLASBuild, 0, vk::PipelineStageFlagBits::eAccelerationStructureBuildKHR);

        transientPool.EndSubmitCommands(commandBuffer);

        times.times = times.stamps.GetResultsNs();
        berry::Log::debug("BLAS node count = {}", blasCounter);
        berry::Log::debug("TLAS node count = {}", scene.nodeGeometry.size());
        berry::Log::debug("BLAS build time = {}", times.times[0]);
        berry::Log::debug("TLAS build time = {}", times.times[1]);
        berry::Log::debug("Sum ns          = {}", times.times[0] + times.times[1]);
    }

private:
    [[nodiscard]] vk::DeviceAddress getBufferAddress(lime::Buffer::Detail const& buffer) const
    {
        return ctx.d.getBufferAddress({ .buffer = buffer.resource }) + buffer.offset;
    }
};

}
