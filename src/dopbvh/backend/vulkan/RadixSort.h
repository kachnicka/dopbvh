#pragma once

#include <radix_sort/platforms/vk/radix_sort_vk.h>
#include <vLime/Capability.h>
#include <vLime/vLime.h>

namespace backend::vulkan {

class FuchsiaRadixSort final : public lime::Capability {
public:
    inline static radix_sort_vk_target_t const* radixSortTarget = nullptr;
    inline static radix_sort_vk_t* radixSort = nullptr;

    static void RadixSortInit(vk::PhysicalDevice pd)
    {
        auto properties { (VkPhysicalDeviceProperties)pd.getProperties() };
        radixSortTarget = radix_sort_vk_target_auto_detect(&properties, 2);
    }
    static void RadixSortCreate(vk::Device d, vk::PipelineCache pCache)
    {
        radixSort = radix_sort_vk_create(d, nullptr, pCache, radixSortTarget);
    }
    static void RadixSortDestroy(vk::Device d)
    {
        radix_sort_vk_destroy(radixSort, d, nullptr);
    }

    [[nodiscard]] char const* getName() const override
    {
        return "Radix Sort (Google Fuchsia)";
    }
    [[nodiscard]] std::vector<char const*> extensionsDevice() const override
    {
        assert(radixSortTarget);

        lime::device::Features dummy;
        radix_sort_vk_target_requirements_t radixReqs {
            .ext_name_count = 0,
            .ext_names = nullptr,
            .pdf = reinterpret_cast<VkPhysicalDeviceFeatures*>(&dummy.features2.features),
            .pdf11 = reinterpret_cast<VkPhysicalDeviceVulkan11Features*>(&dummy.vulkan11Features),
            .pdf12 = reinterpret_cast<VkPhysicalDeviceVulkan12Features*>(&dummy.vulkan12Features),
        };
        radix_sort_vk_target_get_requirements(radixSortTarget, &radixReqs);

        std::vector<char const*> result;
        result.reserve(radixReqs.ext_name_count);
        for (u32 i = 0; i < radixReqs.ext_name_count; i++)
            result.emplace_back(radixReqs.ext_names[i]);
        return result;
    }
    lime::device::Features::Availability checkAndSetDeviceFeatures(lime::device::Features const& available, lime::device::Features& enabled) const override
    {
        assert(radixSortTarget);
        static_cast<void>(available);

        radix_sort_vk_target_requirements_t radixReqs {
            .ext_name_count = 0,
            .ext_names = nullptr,
            .pdf = reinterpret_cast<VkPhysicalDeviceFeatures*>(&enabled.features2.features),
            .pdf11 = reinterpret_cast<VkPhysicalDeviceVulkan11Features*>(&enabled.vulkan11Features),
            .pdf12 = reinterpret_cast<VkPhysicalDeviceVulkan12Features*>(&enabled.vulkan12Features),
        };
        auto const success = radix_sort_vk_target_get_requirements(radixSortTarget, &radixReqs);

        if (success)
            return lime::device::Features::Availability::eAvailable;
        return lime::device::Features::Availability::eNotAvailable;
    }
};

}
