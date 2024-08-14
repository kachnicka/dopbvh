#pragma once

#include <vLime/Memory.h>
#include <vLime/Queues.h>

namespace type_utils {

template<typename Test>
struct is_std_array : std::false_type {
};
template<typename T, size_t N>
struct is_std_array<std::array<T, N>> : std::true_type {
};

template<typename Test, template<typename...> class Ref>
struct is_specialization : std::false_type {
};
template<template<typename...> class Ref, typename... Args>
struct is_specialization<Ref<Args...>, Ref> : std::true_type {
};
template<typename T, template<typename...> class Ref>
inline constexpr bool is_specialization_v = is_specialization<T, Ref>::value;

}

namespace lime {

class Transfer {
    class StagingBuffer;
    std::unique_ptr<StagingBuffer> stagingBuffer;

public:
    Transfer(Queue const& queue, MemoryManager& memory)
        : stagingBuffer(std::make_unique<StagingBuffer>(queue, memory))
    {
    }
    ~Transfer() = default;
    Transfer(Transfer const& rhs) = delete;
    Transfer& operator=(Transfer const& rhs) = delete;
    Transfer(Transfer&& rhs) = default;
    Transfer& operator=(Transfer&& rhs) = default;

    template<typename HostData, typename Resource>
    void ToDeviceSync(HostData const& src, Resource& dst)
    {
        auto [srcPtr, size] { hostDataDetail(src) };
        ToDeviceSync(srcPtr, size, dst);
    }

    template<typename Resource>
    void ToDeviceSync(void const* srcPtr, size_t size, Resource& dst)
    {
        assert(size <= dst.getSizeInBytes());

        if (auto const mapping = dst.getMapping(); mapping != nullptr)
            memcpy(static_cast<char*>(mapping), srcPtr, size);
        else
            stageToDeviceSync(srcPtr, dst, size);
    }

    template<typename HostData, typename Resource>
    void FromDevice(Resource const& src, HostData& dst)
    {
        auto [dstPtr, size] { hostDataDetail(dst) };

        if (auto const mapping = src.getMapping(); mapping != nullptr)
            memcpy(dstPtr, mapping, size);
        else
            stageFromDevice(src, dstPtr, size);
    }

private:
    template<typename HostData>
    [[nodiscard]] std::pair<void const*, size_t> hostDataDetail(HostData const& src)
    {
        if constexpr (std::is_trivially_copy_assignable_v<HostData> && !type_utils::is_std_array<HostData>::value)
            return { &src, sizeof(HostData) };
        else if constexpr (type_utils::is_specialization_v<HostData, std::vector>)
            return { src.data(), src.size() * sizeof(typename HostData::value_type) };
        else {
            unsupportedType<HostData>();
            return {};
        }
    }

    template<typename HostData>
    [[nodiscard]] std::pair<void*, size_t> hostDataDetail(HostData& src)
    {
        if constexpr (std::is_trivially_copy_assignable_v<HostData> && !type_utils::is_std_array<HostData>::value)
            return { &src, sizeof(HostData) };
        else if constexpr (type_utils::is_specialization_v<HostData, std::vector>)
            return { src.data(), src.size() * sizeof(typename HostData::value_type) };
        else {
            unsupportedType<HostData>();
            return {};
        }
    }

    template<typename HostData>
    static void assignToDeviceSync(HostData const& src, void* dst)
    {
        *static_cast<HostData*>(dst) = src;
    }

    template<typename HostData>
    static void memcpyToDeviceSync(HostData const& src, void* dst, std::size_t const size)
    {
        memcpy(dst, src.data(), size);
    }

    template<typename Resource>
    void stageToDeviceSync(void const* src, Resource& dst, std::size_t const size)
    {
        stagingBuffer->stageToDeviceSync(src, dst, size);
    }

    template<typename Resource>
    void stageFromDevice(Resource const& src, void* dst, std::size_t const size)
    {
        stagingBuffer->stageFromDevice(src, dst, size);
    }

    class StagingBuffer {
        static constexpr u32 STAGING_SEGMENT_COUNT = 16u;
        static constexpr vk::DeviceSize STAGING_SEGMENT_SIZE = 4 * MB;
        static constexpr vk::DeviceSize STAGING_BUFFER_SIZE = STAGING_SEGMENT_COUNT * STAGING_SEGMENT_SIZE;

        vk::Device d;

        Buffer stagingBuffer;

        vk::UniqueCommandPool commandPool;
        vk::Queue queue;
        vk::UniqueFence fence;
        vk::CommandBuffer commandBuffer;

    public:
        explicit StagingBuffer(Queue const& queue, MemoryManager& memory)
            : d(memory.d)
            , stagingBuffer(allocStagingBuffer(memory))
            , queue(queue.q)
        {
            vk::CommandPoolCreateInfo poolInfo;
            poolInfo.queueFamilyIndex = queue.queueFamilyIndex;
            poolInfo.flags = vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer;
            commandPool = lime::check(d.createCommandPoolUnique(poolInfo));

            vk::CommandBufferAllocateInfo allocInfo;
            allocInfo.level = vk::CommandBufferLevel::ePrimary;
            allocInfo.commandPool = commandPool.get();
            allocInfo.commandBufferCount = 1;
            commandBuffer = lime::check(d.allocateCommandBuffers(allocInfo)).back();

            fence = FenceFactory(d, vk::FenceCreateFlagBits::eSignaled);
        }

        template<typename Resource>
        void stageToDeviceSync(void const* src, Resource& dst, std::size_t size)
        {
            vk::DeviceSize dataOffset = 0;

            // TODO: most of the workload is shared with regular copying, unify
            // allow image layout noShader without available data
            if (!src) {
                commandBuffer.reset(vk::CommandBufferResetFlags());

                vk::CommandBufferBeginInfo beginInfo;
                beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
                check(commandBuffer.begin(&beginInfo));
                dst.CopyBufferToMe(commandBuffer, { vk::Buffer {}, 0, 0, nullptr }, 0);
                check(commandBuffer.end());

                vk::SubmitInfo submitInfo;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &commandBuffer;

                check(d.resetFences(1, &fence.get()));
                check(queue.submit(1, &submitInfo, fence.get()));

                check(d.waitForFences(1, &fence.get(), vk::True, std::numeric_limits<u64>::max()));
                return;
            }

            while (size > 0) {
                auto const [offsetShift, maxTransferableSize] = dst.getTransferableRegion(0, STAGING_BUFFER_SIZE);
                auto const sizeToTransfer = std::min(maxTransferableSize, size);

                memcpy(stagingBuffer.getMapping(), static_cast<char const*>(src) + dataOffset, sizeToTransfer);
                commandBuffer.reset(vk::CommandBufferResetFlags());

                vk::CommandBufferBeginInfo beginInfo;
                beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
                check(commandBuffer.begin(&beginInfo));
                dst.CopyBufferToMe(commandBuffer, { stagingBuffer.get(), offsetShift, sizeToTransfer, nullptr }, dataOffset);
                check(commandBuffer.end());

                vk::SubmitInfo submitInfo;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &commandBuffer;

                check(d.resetFences(1, &fence.get()));
                check(queue.submit(1, &submitInfo, fence.get()));

                check(d.waitForFences(1, &fence.get(), vk::True, std::numeric_limits<u64>::max()));

                size -= sizeToTransfer;
                dataOffset += sizeToTransfer;
            }
        }

        template<typename Resource>
        void stageFromDevice(Resource const& src, void* dst, std::size_t size)
        {
            vk::DeviceSize dataOffset = 0;
            while (size > 0) {
                auto const [offsetShift, maxTransferableSize] = src.getTransferableRegion(0, STAGING_BUFFER_SIZE);
                auto const sizeToTransfer = std::min(maxTransferableSize, size);

                commandBuffer.reset(vk::CommandBufferResetFlags());

                vk::CommandBufferBeginInfo beginInfo;
                beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
                check(commandBuffer.begin(&beginInfo));
                src.CopyMeToBuffer(commandBuffer, { stagingBuffer.get(), offsetShift, sizeToTransfer, nullptr }, dataOffset);
                check(commandBuffer.end());

                vk::SubmitInfo submitInfo;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &commandBuffer;

                check(d.resetFences(1, &fence.get()));
                check(queue.submit(1, &submitInfo, fence.get()));
                check(d.waitForFences(1, &fence.get(), vk::True, std::numeric_limits<u64>::max()));

                memcpy(static_cast<char*>(dst) + dataOffset, stagingBuffer.getMapping(), sizeToTransfer);

                size -= sizeToTransfer;
                dataOffset += sizeToTransfer;
            }
        }

    private:
        static Buffer allocStagingBuffer(MemoryManager& memory)
        {
            return memory.alloc({ .memoryUsage = DeviceMemoryUsage::eHostToDeviceOptimal },
                {
                    .size = STAGING_BUFFER_SIZE,
                    .usage = vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst,
                },
                "transfer_staging");
        }
    };

    template<typename T>
    static void unsupportedType()
    {
        static_assert(!sizeof(T), "Trying to use unsupported type for GPU data transfer. Supported types: std::vector, trivially assignable types except std::array.");
    }
};

}
