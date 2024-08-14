#pragma once

#include <vLime/Capability.h>
#include <vLime/Queues.h>
#include <vLime/Util.h>
#include <vLime/Vulkan.h>

namespace lime {

namespace device {
class Features;
}

void LoadVulkan();

class Instance {
    vk::UniqueInstance instance {};
    vk::UniqueDebugUtilsMessengerEXT debugMessenger {};

public:
    explicit Instance(Capabilities& capabilities);

    [[nodiscard]] vk::Instance get() const
    {
        return instance.get();
    }
};

class Device {
    vk::PhysicalDevice pDevice;
    vk::UniqueDevice device;

public:
    Queues queues;
    vk::Format preferredDepthFormat_TMP { vk::Format::eUndefined };

    Device(vk::PhysicalDevice pd, Capabilities& capabilities);
    ~Device() = default;
    Device(Device const& rhs) = delete;
    Device& operator=(Device const& rhs) = delete;
    Device(Device&& rhs) = default;
    Device& operator=(Device&& rhs) = default;

    [[nodiscard]] vk::Device get() const
    {
        return device.get();
    }

    [[nodiscard]] vk::PhysicalDevice getPd() const
    {
        return pDevice;
    }

    void WaitIdle() const
    {
        check(device.get().waitIdle());
    };

    [[nodiscard]] bool IsFormatSupported(vk::Format format, vk::FormatFeatureFlags features, vk::ImageTiling tiling = vk::ImageTiling::eOptimal) const
    {
        vk::FormatProperties formatProperties { pDevice.getFormatProperties(format) };
        switch (tiling) {
        case vk::ImageTiling::eOptimal:
            return (formatProperties.optimalTilingFeatures & features) == features;
        case vk::ImageTiling::eLinear:
            return (formatProperties.linearTilingFeatures & features) == features;
        default:
            return false;
        }
    }

private:
    vk::Format getPreferredDepthFormat() const
    {
        std::array allDepthFormats = {
            vk::Format::eD32Sfloat,
            vk::Format::eD16Unorm,
            vk::Format::eD24UnormS8Uint,
            vk::Format::eD16UnormS8Uint,
            vk::Format::eD32SfloatS8Uint,
        };
        for (auto format : allDepthFormats) {
            if (IsFormatSupported(format, vk::FormatFeatureFlagBits::eDepthStencilAttachment))
                return format;
        }
        return vk::Format::eUndefined;
    }
};

[[nodiscard]] std::vector<std::vector<vk::PhysicalDevice>> ListAllPhysicalDevicesInGroups(vk::Instance i);

}
