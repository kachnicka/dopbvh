#pragma once

#include <format>
#include <vLime/Memory.h>
#include <vLime/Platform.h>
#include <vLime/RenderGraph.h>
#include <vLime/vLime.h>

namespace lime {

class SwapChain {
public:
    SwapChain(vk::Instance i, vk::Device d, vk::PhysicalDevice pd, Window const& w, uint8_t multipleBuffering = 2)
        : d(d)
        , pd(pd)
        , surface(getUniqueVulkanSurface(i, w))
        , multipleBuffering(multipleBuffering)
    {
        // if (!v.pd.getSurfaceSupportKHR(vImpl.device.queues.graphics.queueFamilyIndex, surface.get(), v.dispatcher))
        //     throw;
        //     surface.reset();

        createSwapChain();
    }

    Image::Detail GetNextImage(vk::Semaphore semaphore, vk::Fence fence = {}, uint64_t timeout = std::numeric_limits<uint64_t>::max() - 1)
    {
        auto const result { d.acquireNextImageKHR(swapChain.get(), timeout, semaphore, fence) };
        switch (result.result) {
        case vk::Result::eSuccess:
            imageIndexToPresentNext = result.value;
            break;
        case vk::Result::eTimeout:
        case vk::Result::eNotReady:
        case vk::Result::eSuboptimalKHR:
        case vk::Result::eErrorOutOfDateKHR:
        default:
            imageIndexToPresentNext = INVALID_ID;
            return {};
            break;
        }
        return { images[result.value], imageViews[result.value].get(), vk::Extent3D { extent.width, extent.height, 1 }, format };
    }

    [[nodiscard]] std::vector<Image::Detail> GetAllImages() const
    {
        std::vector<Image::Detail> result(images.size());
        for (size_t i = 0; i < result.size(); i++)
            result[i] = { images[i], imageViews[i].get(), vk::Extent3D { extent.width, extent.height, 1 }, format };
        return result;
    }

    vk::Extent2D Resize()
    {
        createSwapChain();
        return extent;
    }

    bool Present(vk::Queue presentQ, std::vector<vk::Semaphore> const& waitSemaphores) const
    {
        if (imageIndexToPresentNext == INVALID_ID)
            return false;

        vk::PresentInfoKHR presentInfo;
        presentInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
        presentInfo.pWaitSemaphores = waitSemaphores.data();

        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapChain.get();
        presentInfo.pImageIndices = &imageIndexToPresentNext;

        auto const result { presentQ.presentKHR(presentInfo) };
        switch (result) {
        case vk::Result::eSuccess:
            return true;
        case vk::Result::eSuboptimalKHR:
        case vk::Result::eErrorOutOfDateKHR:
        default:
            return false;
        }
    }

    struct {
        std::pair<vk::AccessFlagBits, vk::AccessFlagBits> accessFlags;
        vk::PipelineStageFlags srcStage { vk::PipelineStageFlagBits::eTopOfPipe };
        vk::PipelineStageFlags dstStage { vk::PipelineStageFlagBits::eColorAttachmentOutput };
        vk::ImageLayout finalLayout;
    } transitionInfo;

    // TODO: store transition data differently, maybe in lambda capture?
    void scheduleTransitionToRenderGraph(rg::Graph& rg, vk::ImageLayout finalLayout)
    {
        transitionInfo.finalLayout = finalLayout;
        transitionInfo.accessFlags = img_util::DefaultAccessFlags(layout, transitionInfo.finalLayout);
        auto const taskId { rg.AddTask<rg::NoShader>() };
        rg.GetTask(taskId).RegisterExecutionCallback([this](vk::CommandBuffer commandBuffer) {
            vk::ImageMemoryBarrier memoryBarrier {
                .srcAccessMask = transitionInfo.accessFlags.first,
                .dstAccessMask = transitionInfo.accessFlags.second,
                .oldLayout = layout,
                .newLayout = transitionInfo.finalLayout,
                .subresourceRange = {
                    .aspectMask = vk::ImageAspectFlagBits::eColor,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            };
            for (auto const& image : images) {
                memoryBarrier.image = image;
                commandBuffer.pipelineBarrier(transitionInfo.srcStage, transitionInfo.dstStage, {}, nullptr, nullptr, memoryBarrier);
            }
            layout = transitionInfo.finalLayout;
        });
    }

    std::vector<vk::UniqueImageView> imageViews;
    std::vector<vk::Image> images;
    uint32_t imageIndexToPresentNext { INVALID_ID };

private:
    vk::Device d;
    vk::PhysicalDevice pd;

public:
    vk::Format format { vk::Format::eUndefined };
    vk::Extent2D extent;
    vk::ImageLayout layout { vk::ImageLayout::eUndefined };
    vk::UniqueSurfaceKHR surface;
    uint8_t multipleBuffering = 2;
    bool vSync { true };

private:
    vk::UniqueSwapchainKHR swapChain;

    struct SwapChainSupportDetails {
        SwapChainSupportDetails(vk::PhysicalDevice pd, vk::SurfaceKHR surface)
            : capabilities(check(pd.getSurfaceCapabilitiesKHR(surface)))
            , formats(check(pd.getSurfaceFormatsKHR(surface)))
            , presentModes(check(pd.getSurfacePresentModesKHR(surface)))
        {
        }
        vk::SurfaceCapabilitiesKHR capabilities;
        std::vector<vk::SurfaceFormatKHR> formats;
        std::vector<vk::PresentModeKHR> presentModes;
    };

    void createSwapChain()
    {
        SwapChainSupportDetails swapChainSupport { pd, surface.get() };

        auto const surfaceFormat { chooseSwapSurfaceFormat(swapChainSupport.formats) };
        format = surfaceFormat.format;
        extent = chooseSwapExtent(swapChainSupport.capabilities);
        auto const [presentMode, imageCount] { chooseSwapPresentMode(swapChainSupport.presentModes, swapChainSupport.capabilities, multipleBuffering, vSync) };

        // TODO: usage flags based on rt/rasterizer target usage?
        vk::SwapchainCreateInfoKHR createInfo {
            .surface = surface.get(),
            .minImageCount = imageCount,
            .imageFormat = surfaceFormat.format,
            .imageColorSpace = surfaceFormat.colorSpace,
            .imageExtent = extent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eStorage,
            .preTransform = swapChainSupport.capabilities.currentTransform,
            .presentMode = presentMode,
            .clipped = vk::True,
            .oldSwapchain = swapChain.get()
        };
        // vk::SwapchainKHR oldSwapChain { swapChain.get() };
        // createInfo.oldSwapchain = oldSwapChain;

        // auto newSwapChain { d.createSwapchainKHRUnique(createInfo) };
        // swapChain = std::move(newSwapChain);
        swapChain = check(d.createSwapchainKHRUnique(createInfo));
        images = check(d.getSwapchainImagesKHR(swapChain.get()));

        for (size_t i = 0; i < images.size(); i++)
            debug::SetObjectName(images[i], std::format("swap_chain_{}", i).c_str(), d);

        imageViews.clear();
        imageViews.reserve(images.size());
        vk::ImageViewCreateInfo viewCreateInfo {
            .viewType = vk::ImageViewType::e2D,
            .format = format,
            .subresourceRange = {
                .aspectMask = vk::ImageAspectFlagBits::eColor,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        for (auto const& image : images) {
            viewCreateInfo.image = image;
            imageViews.emplace_back(check(d.createImageViewUnique(viewCreateInfo)));
        }
        layout = vk::ImageLayout::eUndefined;
    }

    static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(std::vector<vk::SurfaceFormatKHR> const& availableFormats)
    {
        if (availableFormats.size() == 1 && availableFormats[0].format == vk::Format::eUndefined)
            return { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear };

        for (auto const& availableFormat : availableFormats)
            if (availableFormat.format == vk::Format::eB8G8R8A8Unorm && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
                return availableFormat;

        return availableFormats[0];
    }

    static std::pair<vk::PresentModeKHR, unsigned>
    chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const& availablePresentModes, vk::SurfaceCapabilitiesKHR const& capabilities, uint8_t multipleBuffering, bool vSync)
    {
        auto imageCount { std::max(capabilities.minImageCount, static_cast<uint32_t>(multipleBuffering)) };

        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
            imageCount = capabilities.maxImageCount;

        // TODO: mailbox uses vsync for frame presentation, but does not block rendering of next frames
        //  immediate mode is true no vSync (with possible tearing), consider exposing modes in gui q
        if (!vSync) {
            for (auto const& availablePresentMode : availablePresentModes)
                if (availablePresentMode == vk::PresentModeKHR::eMailbox)
                    return { availablePresentMode, imageCount };
            for (auto const& availablePresentMode : availablePresentModes)
                if (availablePresentMode == vk::PresentModeKHR::eImmediate)
                    return { availablePresentMode, imageCount };
        }

        if (multipleBuffering >= 3)
            log::info("Triple buffering is not available. Switching to double buffering.");
        return { vk::PresentModeKHR::eFifo, imageCount };
    }

    // FALSE POSITIVE VALIDATION LAYER WARNING
    // https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/1340
    static vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const& capabilities)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
            return capabilities.currentExtent;

        vk::Extent2D defaultExtent {
            .width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, 800u)),
            .height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, 600u)),
        };

        return defaultExtent;
    }
};

}
