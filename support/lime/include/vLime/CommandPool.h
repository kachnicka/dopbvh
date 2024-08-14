#pragma once

#include <vLime/Queues.h>

namespace lime::commands {

class TransientPool {
    vk::Device d;
    vk::Queue q;
    vk::UniqueCommandPool pool;

public:
    TransientPool(vk::Device d, lime::Queue queue)
        : d(d)
        , q(queue.q)
    {
        vk::CommandPoolCreateInfo poolCreateInfo {
            .flags = vk::CommandPoolCreateFlagBits::eTransient,
            .queueFamilyIndex = queue.queueFamilyIndex,
        };
        pool = check(d.createCommandPoolUnique(poolCreateInfo));
    }

    [[nodiscard]] vk::CommandBuffer BeginCommands() const
    {
        vk::CommandBufferAllocateInfo allocInfo {
            .commandPool = pool.get(),
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        };

        vk::CommandBuffer commandBuffer;
        check(d.allocateCommandBuffers(&allocInfo, &commandBuffer));

        vk::CommandBufferBeginInfo beginInfo {
            .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit,
        };
        check(commandBuffer.begin(&beginInfo));

        return commandBuffer;
    }

    void EndSubmitCommands(vk::CommandBuffer commandBuffer, vk::Fence fence = {}) const
    {
        check(commandBuffer.end());

        vk::SubmitInfo submitInfo {
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
        };

        if (fence)
            check(d.resetFences(1, &fence));

        check(q.submit(1, &submitInfo, fence));

        if (fence)
            check(d.waitForFences(1, &fence, VK_TRUE, UINT64_MAX));
        else
            check(q.waitIdle());

        d.freeCommandBuffers(pool.get(), 1, &commandBuffer);
    }
};

}
