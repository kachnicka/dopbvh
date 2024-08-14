#pragma once

#include <vLime/Util.h>

namespace lime {
struct Queue {
    Queue() = default;
    Queue(u32 queueFamilyIndex, u32 queueIndex)
        : queueFamilyIndex(queueFamilyIndex)
        , queueIndex(queueIndex)
    {
    }

    void setupQueue(vk::Device d)
    {
        if (isValid(queueFamilyIndex) && isValid(queueIndex))
            q = d.getQueue(queueFamilyIndex, queueIndex);
    }

    u32 queueFamilyIndex { INVALID_ID };
    u32 queueIndex { INVALID_ID };
    vk::Queue q;
};

struct Queues {
    Queues& setupQueues(vk::Device d)
    {
        graphics.setupQueue(d);
        transfer.setupQueue(d);
        asyncCompute.setupQueue(d);
        asyncTransfer.setupQueue(d);
        return *this;
    }

    Queue graphics;
    Queue transfer;
    Queue asyncCompute;
    Queue asyncTransfer;
};
}