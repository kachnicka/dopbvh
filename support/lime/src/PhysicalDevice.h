#pragma once

#include <vector>
#include <vLime/Vulkan.h>
#include <vLime/Queues.h>
#include <vLime/types.h>

namespace lime {
struct Queues;

struct QueueFamilies {
    u32 fUniversal { INVALID_ID };
    u32 fCompute { INVALID_ID };
    u32 fTransfer { INVALID_ID };

private:
    std::vector<u32> availableQueuesCount;
    std::vector<u32> nextFreeQueue;
    std::vector<std::vector<f32>> requestedQueuePriorities;

    u32 getFreeQueueID(u32 queueFamilyId, f32 priority = 0.5f)
    {
        if (nextFreeQueue[queueFamilyId] + 1 <= availableQueuesCount[queueFamilyId]) {
            requestedQueuePriorities[queueFamilyId].emplace_back(priority);
            return nextFreeQueue[queueFamilyId]++;
        }
        return nextFreeQueue[queueFamilyId] - 1;
    }

public:
    explicit QueueFamilies(vk::PhysicalDevice pd)
    {
        u32 i { 0 };
        for (auto const& qProp : pd.getQueueFamilyProperties()) {
            if (qProp.queueCount > 0) {
                if (checkVkFlags(qProp.queueFlags, vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer))
                    fUniversal = i;
                else if (checkVkFlags(qProp.queueFlags, vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer))
                    fCompute = i;
                else if (checkVkFlags(qProp.queueFlags, vk::QueueFlagBits::eTransfer))
                    fTransfer = i;
                else
                    continue;

                availableQueuesCount.emplace_back(qProp.queueCount);
                nextFreeQueue.emplace_back(0);
                requestedQueuePriorities.emplace_back();
            }
            if (isValid(fUniversal) && isValid(fCompute) && isValid(fTransfer))
                break;
            i++;
        }
    }

    std::pair<std::vector<vk::DeviceQueueCreateInfo>, Queues> PrepareQueues()
    {
        Queues queues;
        if (fUniversal != std::numeric_limits<u32>::max()) {
            queues.graphics = Queue(fUniversal, getFreeQueueID(fUniversal));
            queues.transfer = Queue(fUniversal, getFreeQueueID(fUniversal));
        }
        if (fCompute != std::numeric_limits<u32>::max())
            queues.asyncCompute = Queue(fCompute, getFreeQueueID(fCompute));
        if (fTransfer != std::numeric_limits<u32>::max())
            queues.asyncTransfer = Queue(fTransfer, getFreeQueueID(fTransfer));

        std::vector<vk::DeviceQueueCreateInfo> cInfos;
        u32 queueFamilyId = 0;
        for (auto const& queuePriority : requestedQueuePriorities) {
            if (!queuePriority.empty())
                cInfos.push_back({ .queueFamilyIndex = queueFamilyId, .queueCount = csize<u32>(queuePriority), .pQueuePriorities = queuePriority.data() });
            queueFamilyId++;
        }

        return std::make_pair(cInfos, queues);
    }
};
}
