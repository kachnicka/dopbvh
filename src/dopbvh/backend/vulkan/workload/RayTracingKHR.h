#pragma once

#include "../VCtx.h"
#include <glm/ext/vector_float4.hpp>
#include <vLime/Memory.h>
#include <vLime/Reflection.h>
#include <vLime/Timestamp.h>
#include <vLime/Transfer.h>
#include <vLime/vLime.h>

namespace backend::vulkan {

class PathTracingKHR {
    VCtx ctx;

    struct Pipeline {
        [[nodiscard]] constexpr static std::vector<std::string_view> GetShaderNames()
        {
            return {
                "pathtrace.rgen.spv",
                "pathtrace.rmiss.spv",
                "pathtraceShadow.rmiss.spv",
                "pathtrace.rchit.spv",
            };
        }
    };
    lime::PipelineRayTracing pipeline;

    vk::UniqueDescriptorPool dPool;
    vk::DescriptorSet dSetRayTracing;

    lime::Buffer sbt;
    bool resetRayCounter { true };

public:
    lime::Buffer rayAtomicCounter;
    explicit PathTracingKHR(VCtx ctx)
        : ctx(ctx)
        , pipeline(setupPipeline(ctx.d, ctx.sCache))
        , dPool(getDPool())
        , dSetRayTracing(getDSet())
        , times(ctx.d, ctx.pd)
    {
        pipeline.build();
        createSBT();

        rayAtomicCounter = ctx.memory.alloc({ lime::DeviceMemoryUsage::eHostToDeviceOptimal },
            {
                .size = sizeof(u64),
                .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eTransferSrc,
            },
            "ray_atomic_counter");
    }

    void updateDSet(vk::ImageView targetImageView, vk::AccelerationStructureKHR tlas, lime::Buffer::Detail const& sceneDescription, lime::Buffer::Detail const& camera)
    {
        // TODO: move descriptor set update
        vk::WriteDescriptorSetAccelerationStructureKHR dTLAS[1];
        dTLAS[0].accelerationStructureCount = 1;
        dTLAS[0].pAccelerationStructures = &tlas;

        vk::DescriptorImageInfo dImage[1];
        dImage[0].imageView = targetImageView;
        dImage[0].imageLayout = vk::ImageLayout::eGeneral;

        vk::DescriptorBufferInfo dBuffer[1];
        dBuffer[0].buffer = camera.resource;
        dBuffer[0].offset = camera.offset;
        dBuffer[0].range = camera.size;

        vk::DescriptorBufferInfo sBuffer[1];
        sBuffer[0].buffer = sceneDescription.resource;
        sBuffer[0].offset = sceneDescription.offset;
        sBuffer[0].range = sceneDescription.size;

        vk::DescriptorBufferInfo racBuffer[1];
        racBuffer[0].buffer = rayAtomicCounter.get();
        racBuffer[0].offset = 0;
        racBuffer[0].range = rayAtomicCounter.getSizeInBytes();

        vk::WriteDescriptorSet dWrite[5];
        dWrite[0].dstSet = dSetRayTracing;
        dWrite[0].dstBinding = 0;
        dWrite[0].descriptorCount = 1;
        dWrite[0].descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
        dWrite[0].pNext = dTLAS;

        dWrite[1].dstSet = dSetRayTracing;
        dWrite[1].dstBinding = 1;
        dWrite[1].descriptorCount = 1;
        dWrite[1].descriptorType = vk::DescriptorType::eStorageImage;
        dWrite[1].pImageInfo = dImage;

        dWrite[2].dstSet = dSetRayTracing;
        dWrite[2].dstBinding = 2;
        dWrite[2].descriptorCount = 1;
        dWrite[2].descriptorType = vk::DescriptorType::eUniformBufferDynamic;
        dWrite[2].pBufferInfo = dBuffer;

        dWrite[3].dstSet = dSetRayTracing;
        dWrite[3].dstBinding = 3;
        dWrite[3].descriptorCount = 1;
        dWrite[3].descriptorType = vk::DescriptorType::eStorageBufferDynamic;
        dWrite[3].pBufferInfo = sBuffer;

        dWrite[4].dstSet = dSetRayTracing;
        dWrite[4].dstBinding = 4;
        dWrite[4].descriptorCount = 1;
        dWrite[4].descriptorType = vk::DescriptorType::eStorageBufferDynamic;
        dWrite[4].pBufferInfo = racBuffer;

        ctx.d.updateDescriptorSets(5, dWrite, 0, nullptr);
    }

    u64 RayCounterReadBackAndReset()
    {
        u64 result { 0 };
        ctx.transfer.FromDevice(rayAtomicCounter, result);
        resetRayCounter = true;
        return result;
    }

    struct Samples_TMP {
        u32 computed { 0 };
        u32 toCompute { 0 };
    };

    glm::vec4 dirLight_TMP;

    void recordCommands(vk::CommandBuffer commandBuffer, u32 x, u32 y, Samples_TMP const& samples)
    {
        if (resetRayCounter) {
            resetRayCounter = false;
            commandBuffer.fillBuffer(rayAtomicCounter.get(), 0, 8, 0);
            vk::MemoryBarrier writeMemoryBarrier { .srcAccessMask = vk::AccessFlagBits::eTransferWrite, .dstAccessMask = vk::AccessFlagBits::eShaderRead };
            commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eRayTracingShaderKHR, vk::DependencyFlags(), writeMemoryBarrier, nullptr, nullptr);
        }

        lime::debug::BeginDebugLabel(commandBuffer, "path tracing");
        times.stamps.Reset(commandBuffer);

        std::vector<vk::DescriptorSet> desc_set { dSetRayTracing };
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, pipeline.get());
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eRayTracingKHR, pipeline.getLayout(), 0, desc_set, { 0, 0, 0 });

        commandBuffer.pushConstants(pipeline.getLayout(), vk::ShaderStageFlagBits::eRaygenKHR, 0, 4 * sizeof(f32), &dirLight_TMP);
        commandBuffer.pushConstants(pipeline.getLayout(), vk::ShaderStageFlagBits::eRaygenKHR, 4 * sizeof(f32), 2 * sizeof(u32), &samples);

        // TODO: cache necessary values to avoid this query for each redraw
        auto const rtProperties = ctx.pd.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>().get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

        // Size of a program identifier
        vk::DeviceAddress const groupSize = lime::memory::align(rtProperties.shaderGroupHandleSize, rtProperties.shaderGroupBaseAlignment);
        vk::DeviceAddress const groupStride = groupSize;

        vk::DeviceAddress const sbtAddress { ctx.d.getBufferAddress({ .buffer = sbt.get() }) };
        using Stride = vk::StridedDeviceAddressRegionKHR;
        std::array<Stride, 4> const strideAddresses {
            Stride { sbtAddress + 0u * groupSize, groupStride, groupSize * 1 }, // raygen
            Stride { sbtAddress + 1u * groupSize, groupStride, groupSize * 2 }, // miss
            Stride { sbtAddress + 3u * groupSize, groupStride, groupSize * 1 }, // hit
            Stride { 0u, 0u, 0u }                                               // callable
        };
        times.stamps.WriteBeginStamp(commandBuffer, vk::PipelineStageFlagBits::eRayTracingShaderKHR);
        commandBuffer.traceRaysKHR(&strideAddresses[0], &strideAddresses[1], &strideAddresses[2], &strideAddresses[3], x, y, 1);
        times.stamps.Write(commandBuffer, Times::Stamp::ePathTracing, 0, vk::PipelineStageFlagBits::eRayTracingShaderKHR);

        lime::debug::EndDebugLabel(commandBuffer);
    }

private:
    [[nodiscard]] vk::UniqueDescriptorPool getDPool() const
    {
        std::vector<vk::DescriptorPoolSize> poolSizes {
            { vk::DescriptorType::eAccelerationStructureKHR, 10 },
            { vk::DescriptorType::eStorageImage, 10 },
            { vk::DescriptorType::eUniformBuffer, 10 },
        };
        vk::DescriptorPoolCreateInfo cInfo;
        cInfo.maxSets = 30;
        cInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
        cInfo.pPoolSizes = poolSizes.data();
        return lime::check(ctx.d.createDescriptorPoolUnique(cInfo));
    }

    [[nodiscard]] vk::DescriptorSet getDSet() const
    {
        vk::DescriptorSetAllocateInfo allocInfo;
        allocInfo.descriptorPool = dPool.get();
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &pipeline.layout.dSet[0].get();
        return lime::check(ctx.d.allocateDescriptorSets(allocInfo)).back();
    }

    void createSBT()
    {
        auto const rtProperties = ctx.pd.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>().get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();
        //
        // TODO: derive group count form the pipeline
        auto constexpr groupCount { 4u };
        auto const groupHandleSize { rtProperties.shaderGroupHandleSize };
        auto const groupSizeAligned { lime::memory::align(groupHandleSize, rtProperties.shaderGroupBaseAlignment) };
        auto const sbtSize = groupCount * groupSizeAligned;

        std::vector<uint8_t> shaderHandleStorage(sbtSize);
        lime::check(ctx.d.getRayTracingShaderGroupHandlesKHR(pipeline.get(), 0, groupCount, sbtSize, shaderHandleStorage.data()));

        auto handlesCopy = shaderHandleStorage;
        for (unsigned g = 0; g < groupCount; ++g)
            memcpy(shaderHandleStorage.data() + g * groupSizeAligned, handlesCopy.data() + g * groupHandleSize, groupHandleSize);

        if (sbtSize > sbt.getSizeInBytes()) {
            sbt.reset();
            sbt = ctx.memory.alloc({ lime::DeviceMemoryUsage::eDeviceOptimal },
                {
                    .size = sbtSize,
                    .usage = vk::BufferUsageFlagBits::eShaderBindingTableKHR | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
                },
                "rt_sbt");
        }
        ctx.transfer.ToDeviceSync(shaderHandleStorage, sbt);
    }

    [[nodiscard]] lime::PipelineRayTracing setupPipeline(vk::Device d, lime::ShaderCache& sCache)
    {
        lime::PipelineRayTracing prt { d, sCache, Pipeline::GetShaderNames() };
        return prt;
    }

public:
    struct Times {
        enum class Stamp : u32 {
            ePathTracing,
            eCount,
        };

        [[nodiscard]] inline static std::string to_string(Stamp name)
        {
            switch (name) {
            case Stamp::ePathTracing:
                return "Path tracing shader";
            default:
                return "n/a";
            }
        }

        Times(vk::Device d, vk::PhysicalDevice pd)
            : stamps(d, pd)
        {
        }

        lime::Timestamps<Times::Stamp> stamps;

        std::vector<f32> total;
        f32 sum { 0.f };

        void computeSum()
        {
            sum = 0.f;
            for (auto const& v : total)
                sum += v;
        }
    } times;
};
}
