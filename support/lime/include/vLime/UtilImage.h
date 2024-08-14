#pragma once

#include <vLime/vLime.h>

namespace lime::img_util {

// https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/html/vkspec.html#formats-compatibility
[[nodiscard]] inline vk::DeviceSize BytesPerPixel(vk::Format format)
{
    switch (format) {
    case vk::Format::eUndefined:
        return 0u;
    case vk::Format::eR8Uint:
    case vk::Format::eR8Unorm:
    case vk::Format::eS8Uint:
        return 1u;
    case vk::Format::eR16Sfloat:
    case vk::Format::eD16Unorm:
        return 2u;
    case vk::Format::eB8G8R8Unorm:
        return 3u;
    case vk::Format::eR32Uint:
    case vk::Format::eR16G16Sfloat:
    case vk::Format::eB10G11R11UfloatPack32:
    case vk::Format::eD32Sfloat:
    case vk::Format::eD24UnormS8Uint:
    case vk::Format::eB8G8R8A8Unorm:
    case vk::Format::eR8G8B8A8Unorm:
    case vk::Format::eB8G8R8A8Srgb:
        return 4u;
    case vk::Format::eD32SfloatS8Uint:
        return 5u;
    case vk::Format::eR32G32Sfloat:
    case vk::Format::eR16G16B16A16Sfloat:
        return 8u;
    case vk::Format::eR32G32B32Sfloat:
        return 12u;
    case vk::Format::eR32G32B32A32Sfloat:
        return 16u;
    default:
        log::error("Unknown image format!");
        assert(0);
        return 0u;
    }
}

[[nodiscard]] inline vk::DeviceSize Size(vk::Extent3D const& extent, vk::Format format)
{
    return extent.width * extent.height * extent.depth * BytesPerPixel(format);
}

[[nodiscard]] inline vk::ImageAspectFlags Aspect(vk::Format format)
{
    switch (format) {
    case vk::Format::eS8Uint:
        return vk::ImageAspectFlagBits::eStencil;
    case vk::Format::eD16UnormS8Uint:
    case vk::Format::eD24UnormS8Uint:
    case vk::Format::eD32SfloatS8Uint:
        return vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil;
    case vk::Format::eD16Unorm:
    case vk::Format::eD32Sfloat:
        return vk::ImageAspectFlagBits::eDepth;
    default:
        return vk::ImageAspectFlagBits::eColor;
    }
}

[[nodiscard]] inline bool IsStencil(vk::Format format)
{
    auto const testedAspect { vk::ImageAspectFlagBits::eStencil };
    return (Aspect(format) & testedAspect) == testedAspect;
}

[[nodiscard]] inline bool IsDepth(vk::Format format)
{
    auto const testedAspect { vk::ImageAspectFlagBits::eDepth };
    return (Aspect(format) & testedAspect) == testedAspect;
}

[[nodiscard]] inline bool IsDepthStencil(vk::Format format)
{
    auto const testedAspect { vk::ImageAspectFlagBits::eDepth | vk::ImageAspectFlagBits::eStencil };
    return (Aspect(format) & testedAspect) == testedAspect;
}

inline std::pair<vk::AccessFlagBits, vk::AccessFlagBits> DefaultAccessFlags(vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
{
    switch (oldLayout) {
    case vk::ImageLayout::eUndefined:
        switch (newLayout) {
        case vk::ImageLayout::eGeneral:
            return { {}, {} };
        case vk::ImageLayout::ePresentSrcKHR:
            return { {}, {} };
        case vk::ImageLayout::eTransferSrcOptimal:
            return { {}, vk::AccessFlagBits::eTransferRead };
        case vk::ImageLayout::eTransferDstOptimal:
            return { {}, vk::AccessFlagBits::eTransferWrite };
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            return { {}, vk::AccessFlagBits::eShaderRead };
        default:
            break;
        }
        break;
    case vk::ImageLayout::eGeneral:
        switch (newLayout) {
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            return { {}, vk::AccessFlagBits::eShaderRead };
        case vk::ImageLayout::eTransferSrcOptimal:
            return { {}, vk::AccessFlagBits::eTransferRead };
        case vk::ImageLayout::ePresentSrcKHR:
            return { {}, {} };
        default:
            break;
        }
        break;
    case vk::ImageLayout::eTransferSrcOptimal:
        switch (newLayout) {
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            return { vk::AccessFlagBits::eTransferRead, vk::AccessFlagBits::eShaderRead };
        case vk::ImageLayout::eGeneral:
            return { vk::AccessFlagBits::eTransferRead, {} };
        default:
            break;
        }
        break;
    case vk::ImageLayout::eTransferDstOptimal:
        switch (newLayout) {
        case vk::ImageLayout::eShaderReadOnlyOptimal:
            return { vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead };
        case vk::ImageLayout::ePresentSrcKHR:
            return { vk::AccessFlagBits::eTransferWrite, {} };
        default:
            break;
        }
        break;
    case vk::ImageLayout::eShaderReadOnlyOptimal:
        switch (newLayout) {
        case vk::ImageLayout::eGeneral:
            return { vk::AccessFlagBits::eShaderRead, {} };
        case vk::ImageLayout::eTransferSrcOptimal:
            return { vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eTransferRead };
        default:
            break;
        }
        break;
    case vk::ImageLayout::ePresentSrcKHR:
        switch (newLayout) {
        case vk::ImageLayout::eTransferDstOptimal:
            return { {}, vk::AccessFlagBits::eTransferWrite };
        case vk::ImageLayout::eGeneral:
            return { {}, {} };
        default:
            break;
        }
        break;
    default:
        break;
    }
    assert(false);
    log::error("Unsupported layout transition :(");// + vk::to_string(oldLayout) + " >> " + vk::to_string(newLayout)));
    return {};
}

}
