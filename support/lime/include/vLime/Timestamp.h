#pragma once

#include <vLime/vLime.h>

#include <utility>

namespace lime {

// TimestampEnumClass type expects last entry in enum class to be called TimestampEnumClass::eCount
// one extra stamp is created to record as the base timestamp
template<typename TimestampEnumClass>
class Timestamps {
    vk::Device d;
    vk::PhysicalDevice pd;
    u32 rangeCount { 0 };
    u32 timestampCount { 0 };
    u32 queryCount { 0 };
    vk::UniqueQueryPool qPool;

public:
    Timestamps(vk::Device d, vk::PhysicalDevice pd, u32 rangeCount = 1)
        : d(d)
        , pd(pd)
        , rangeCount(rangeCount)
        , timestampCount(std::to_underlying(TimestampEnumClass::eCount))
        , queryCount(timestampCount * rangeCount + 1)
    {
        vk::QueryPoolCreateInfo cInfo;
        cInfo.queryType = vk::QueryType::eTimestamp;
        cInfo.queryCount = queryCount;
        qPool = check(d.createQueryPoolUnique(cInfo));
    }

    [[nodiscard]] std::vector<f32> GetResultsNs() const
    {
        std::vector<f32> result(timestampCount, 0.f);
        AddResultsNs(result);
        return result;
    }

    void AddResultsNs(std::vector<f32>& data) const
    {
        auto values {
            check(d.getQueryPoolResults<u64>(
                qPool.get(),
                0,
                queryCount,
                queryCount * static_cast<u32>(sizeof(u64)),
                sizeof(u64),
                vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait))
        };

        // TODO: cache these values in vulkan startup
        auto const timestampValidBits { pd.getQueueFamilyProperties()[0].timestampValidBits };
        auto const timestampPeriod { pd.getProperties().limits.timestampPeriod };

        // if necessary, mask 64bit requested value to valid value
        if (timestampValidBits < sizeof(u64) * 8)
            std::ranges::transform(values.begin(), values.end(), values.begin(), [timestampValidBits](u64 x) {
                return x & ~(static_cast<u64>(-1) << timestampValidBits);
            });

        // convert timestamps to diffs
        for (u32 i { queryCount - 1 }; i > 0; i--)
            values[i] -= values[i - 1];

        // range - multiple stamps
        for (u32 r { 0 }; r < rangeCount; r++)
            for (u32 i { 0 }; i < timestampCount; i++)
                data[i] += static_cast<f32>(values[r * timestampCount + i + 1]) * timestampPeriod;
    }

    [[nodiscard]] f32 GetTimeNs_TMP(u64 stamp) const
    {
        // TODO: cache these values in vulkan startup
        auto const timestampValidBits { pd.getQueueFamilyProperties()[0].timestampValidBits };
        auto const timestampPeriod { pd.getProperties().limits.timestampPeriod };

        // if necessary, mask 64bit requested value to valid value
        if (timestampValidBits < sizeof(u64) * 8)
            stamp = stamp & ~(static_cast<u64>(-1) << timestampValidBits);

        return static_cast<f32>(stamp) * timestampPeriod;
    }

    // TODO: sub-range reset API?
    void Reset(vk::CommandBuffer commandBuffer)
    {
        commandBuffer.resetQueryPool(qPool.get(), 0, queryCount);
    }

    void WriteBeginStamp(vk::CommandBuffer commandBuffer, vk::PipelineStageFlagBits pipelineStage = vk::PipelineStageFlagBits::eComputeShader)
    {
        commandBuffer.writeTimestamp(pipelineStage, qPool.get(), 0);
    }

    void Write(vk::CommandBuffer commandBuffer, TimestampEnumClass stamp, u32 rangeId = 0, vk::PipelineStageFlagBits pipelineStage = vk::PipelineStageFlagBits::eComputeShader)
    {
        commandBuffer.writeTimestamp(pipelineStage, qPool.get(), rangeId * rangeCount + std::to_underlying(stamp) + 1);
    }

private:
};

struct SingleTimer {
    enum class Timer : u32 {
        eTime,
        eCount,
    };

    [[nodiscard]] inline static std::string to_string(Timer name)
    {
        switch (name) {
        case Timer::eTime:
            return "Time";
        default:
            return "n/a";
        }
    }

    explicit SingleTimer(vk::Device d, vk::PhysicalDevice pd)
        : stamp(d, pd)
    {
    }

    lime::Timestamps<Timer> stamp;

    void Reset(vk::CommandBuffer commandBuffer)
    {
        stamp.Reset(commandBuffer);
    }

    void Begin(vk::CommandBuffer commandBuffer, vk::PipelineStageFlagBits pipelineStage = vk::PipelineStageFlagBits::eComputeShader)
    {
        stamp.WriteBeginStamp(commandBuffer, pipelineStage);
    }

    void End(vk::CommandBuffer commandBuffer, vk::PipelineStageFlagBits pipelineStage = vk::PipelineStageFlagBits::eComputeShader)
    {
        stamp.Write(commandBuffer, Timer::eTime, 0, pipelineStage);
    }

    [[nodiscard]] f32 ReadTimeNs()
    {
        return stamp.GetResultsNs()[0];
    }
};

}
