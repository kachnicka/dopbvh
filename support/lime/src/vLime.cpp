#include <vLime/vLime.h>

#include "DebugMessenger.h"
#include "PhysicalDevice.h"
#include <vLime/ExtensionsAndLayers.h>
#include <vLime/Memory.h>
#include <vLime/types.h>
#include <vulkan/vulkan_hpp_macros.hpp>

#if VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1
VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE
#endif

namespace lime {

void LoadVulkan()
{
#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
    VULKAN_HPP_DEFAULT_DISPATCHER.init();
#endif
}

Instance::Instance(Capabilities& capabilities)
{
    auto const iExt { extension::CheckExtensionsInstance(capabilities) };
    auto const vLayers { validation::GetDefaultLayers(capabilities.isAvailable<DebugUtils>()) };

    vk::ApplicationInfo constexpr appInfo {
        .pApplicationName = "Lime App",
        .applicationVersion = vk::makeApiVersion(0, 1, 0, 0),
        .pEngineName = "My Engine",
        .engineVersion = 1,
        .apiVersion = vk::ApiVersion13,
    };

    vk::InstanceCreateInfo const createInfo {
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = csize<u32>(vLayers),
        .ppEnabledLayerNames = vLayers.data(),
        .enabledExtensionCount = csize<u32>(iExt),
        .ppEnabledExtensionNames = iExt.data(),
    };

    instance = check(vk::createInstanceUnique(createInfo));
#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
    VULKAN_HPP_DEFAULT_DISPATCHER.init(get());
#endif

    if (!vLayers.empty())
        debugMessenger = check(debug::CreateDebugMessenger(get()));
}

Device::Device(vk::PhysicalDevice pd, Capabilities& capabilities)
    : pDevice(pd)
    , preferredDepthFormat_TMP(getPreferredDepthFormat())
{
    auto const dExt { extension::CheckExtensionsDevice(capabilities, pd) };
    auto dFeatures { device::CheckAndSetDeviceFeatures(capabilities, pd) };

    QueueFamilies qFamilies { pd };
    auto [qCreateInfos, queuesProxy] { qFamilies.PrepareQueues() };

    vk::DeviceCreateInfo dCreateInfo;
    dCreateInfo.pNext = &dFeatures.GetFeaturesChain();
    dCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(qCreateInfos.size());
    dCreateInfo.pQueueCreateInfos = qCreateInfos.data();
    dCreateInfo.enabledExtensionCount = static_cast<uint32_t>(dExt.size());
    dCreateInfo.ppEnabledExtensionNames = dExt.data();

    device = check(pd.createDeviceUnique(dCreateInfo));

#if (VULKAN_HPP_DISPATCH_LOADER_DYNAMIC == 1)
    VULKAN_HPP_DEFAULT_DISPATCHER.init(get());
#endif

    queues = queuesProxy.setupQueues(get());
}

[[nodiscard]] std::vector<std::vector<vk::PhysicalDevice>> ListAllPhysicalDevicesInGroups(vk::Instance i)
{
    std::vector<std::vector<vk::PhysicalDevice>> list;
    auto const pdGroups { check(i.enumeratePhysicalDeviceGroups()) };

    for (auto const& pdGroup : pdGroups) {
        list.emplace_back();
        for (auto const& pd : pdGroup.physicalDevices)
            if (pd)
                list.back().emplace_back(pd);
    }
    return list;
}

}
