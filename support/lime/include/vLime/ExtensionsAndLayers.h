#pragma once

#include <format>
#include <vLime/Capability.h>
#include <vLime/Util.h>
#include <vLime/Vulkan.h>
#include <vector>

namespace lime::extension {

template<typename fnExtRequested>
std::vector<char const*> GetExtensionsRemoveUnavailableCapabilities(Capabilities& capabilities, std::vector<vk::ExtensionProperties> const& extAvailable, fnExtRequested f)
{
    std::vector<std::string_view> capabilityUnavailable;
    std::vector<char const*> ext;

    for (auto const& c : capabilities) {
        auto const extRequested { f(c) };

        std::vector<char const*> extSupported;
        std::vector<char const*> extUnsupported;
        for (auto const& er : extRequested) {
            if (auto const it {
                    std::find_if(extAvailable.begin(), extAvailable.end(), [&er](auto const& ae) {
                        return strcmp(er, ae.extensionName) == 0;
                    }) };
                it == extAvailable.end()) {
                extUnsupported.emplace_back(er);
            } else {
                extSupported.emplace_back(er);
            }
        }

        if (!extUnsupported.empty())
            capabilityUnavailable.emplace_back(c->getName());
        else
            ext.insert(ext.end(), std::make_move_iterator(extSupported.begin()), std::make_move_iterator(extSupported.end()));

        // gcc std::format does not implement vectors
        // if (!extRequested.empty())
        // Log(std::format("  '{}': {}", c->getName(), extUnsupported.empty() ? "ok" : std::format("disabled - extensions {} not available", extUnsupported)));
        if (!extRequested.empty()) {
            size_t totalSize = 2;
            for (size_t i = 0; i < extUnsupported.size(); ++i) {
                totalSize += strlen(extUnsupported[i]);
                if (i < extUnsupported.size() - 1)
                    totalSize += 2;
            }
            std::string extStr;
            extStr.reserve(totalSize);
            extStr += '[';
            for (size_t i = 0; i < extUnsupported.size(); ++i) {
                extStr += extUnsupported[i];
                if (i < extUnsupported.size() - 1)
                    extStr += ", ";
            }
            extStr += ']';

            log::info(std::format("  '{}': {}", c->getName(), extUnsupported.empty() ? "ok" : std::format("disabled - extensions {} not available", extStr)));
        }
    }

    for (auto const& c : capabilityUnavailable)
        capabilities.remove(c);

    return ext;
}

inline std::vector<char const*> CheckExtensionsInstance(Capabilities& capabilities)
{
    auto const extAvailable { check(vk::enumerateInstanceExtensionProperties()) };
    log::info("Capabilities: instance extensions..");
    return GetExtensionsRemoveUnavailableCapabilities(capabilities, extAvailable, [](auto const& capability) { return capability->extensionsInstance(); });
}

inline std::vector<char const*> CheckExtensionsDevice(Capabilities& capabilities, vk::PhysicalDevice pd)
{
    auto const extAvailable { check(pd.enumerateDeviceExtensionProperties()) };
    log::info("Capabilities: device extensions..");
    return GetExtensionsRemoveUnavailableCapabilities(capabilities, extAvailable, [](auto const& capability) { return capability->extensionsDevice(); });
}

}
namespace lime::validation {

inline void RemoveUnavailableLayers(std::vector<char const*>& layersRequested)
{
    auto const layersAvailable { check(vk::enumerateInstanceLayerProperties()) };
    std::vector<char const*> layersSupported;

    for (auto const& l : layersRequested) {
        if (auto const it = std::find_if(layersAvailable.begin(), layersAvailable.end(), [&l](auto const& la) {
                return strcmp(l, la.layerName) == 0;
            });
            it == layersAvailable.end())
            log::info(std::format("  layer '{}': {}", l, "disabled - not available"));
        else
            layersSupported.push_back(l);
    }
    layersRequested = layersSupported;
}

inline std::vector<char const*> GetDefaultLayers(bool const haveDebugUtils)
{
    if (!haveDebugUtils)
        return {};

    std::vector<char const*> layers {
        "VK_LAYER_KHRONOS_validation",
        // "VK_LAYER_LUNARG_api_dump",
        // "VK_LAYER_LUNARG_object_tracker",
    };
    RemoveUnavailableLayers(layers);
    return layers;
}
}

namespace lime::device {

inline Features CheckAndSetDeviceFeatures(Capabilities& capabilities, vk::PhysicalDevice pd, bool silent = false)
{
    Features enabled;
    Features const available { pd };

    if (!silent)
        log::info("Capabilities: device features..");
    std::vector<std::string_view> capabilityUnavailable;
    for (auto const& c : capabilities) {
        auto const featuresAvailable { c->checkAndSetDeviceFeatures(available, enabled) };
        using av = lime::device::Features::Availability;
        if (featuresAvailable == av::eNotAvailable)
            capabilityUnavailable.emplace_back(c->getName());
        if (featuresAvailable != av::eDontCare)
            if (!silent)
                log::info(std::format("  '{}': {}", c->getName(), featuresAvailable == av::eAvailable ? "ok" : "disabled - features not available"));
    }

    for (auto const& c : capabilityUnavailable)
        capabilities.remove(c);

    return enabled;
}

}
