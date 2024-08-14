#pragma once

#include <limits>
#include <vLime/CommandPool.h>

namespace lime {

class Frame {
    vk::Device d;
    vk::Queue q;
    vk::UniqueCommandPool pool;
    u64 currentFrameId { 0 };

public:
    vk::CommandBuffer commandBuffer;
    vk::UniqueSemaphore imageAvailableSemaphore;
    vk::UniqueSemaphore renderFinishedSemaphore;
    vk::UniqueFence renderFinishedFence;

public:
    Frame(vk::Device d, lime::Queue queue)
        : d(d)
        , q(queue.q)
        , imageAvailableSemaphore(SemaphoreFactory(d))
        , renderFinishedSemaphore(SemaphoreFactory(d))
        , renderFinishedFence(FenceFactory(d, vk::FenceCreateFlagBits::eSignaled))
    {
        vk::CommandPoolCreateInfo poolCreateInfo {
            .flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            .queueFamilyIndex = queue.queueFamilyIndex,
        };
        pool = check(d.createCommandPoolUnique(poolCreateInfo));

        vk::CommandBufferAllocateInfo allocInfo {
            .commandPool = pool.get(),
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1,
        };
        check(d.allocateCommandBuffers(&allocInfo, &commandBuffer));
    }

    void ResetFrameCounter()
    {
        currentFrameId = 0u;
    }

    void Wait() const
    {
        check(d.waitForFences(1, &renderFinishedFence.get(), vk::True, std::numeric_limits<u64>::max()));
    }

    [[nodiscard]] std::pair<u64, vk::CommandBuffer> ResetAndBeginCommandsRecording() const
    {
        commandBuffer.reset(vk::CommandBufferResetFlags());

        vk::CommandBufferBeginInfo beginInfo;
        check(commandBuffer.begin(&beginInfo));

        return { currentFrameId, commandBuffer };
    }

    void EndCommandsRecording() const
    {
        check(commandBuffer.end());
    }

    void SubmitCommands(bool waitSemaphores = true, bool signalSemaphores = true)
    {
        std::vector<vk::PipelineStageFlags> waitStages = { vk::PipelineStageFlagBits::eColorAttachmentOutput };

        vk::SubmitInfo submitInfo {
            .pWaitDstStageMask = waitStages.data(),
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
        };

        if (waitSemaphores) {
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &imageAvailableSemaphore.get();
        }
        if (signalSemaphores) {
            submitInfo.signalSemaphoreCount = 1;
            submitInfo.pSignalSemaphores = &renderFinishedSemaphore.get();
        }

        check(d.resetFences(1, &renderFinishedFence.get()));
        check(q.submit(submitInfo, renderFinishedFence.get()));
        currentFrameId++;
    }
};

}
