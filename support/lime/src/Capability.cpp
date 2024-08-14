#include <vLime/Capability.h>

namespace lime::device {

Features::Features(vk::PhysicalDevice const pd)
{
    auto& features { GetFeaturesChain() };
    pd.getFeatures2(&features);
}

vk::PhysicalDeviceFeatures2& Features::GetFeaturesChain()
{
    features2.pNext = &vulkan11Features;
    vulkan11Features.pNext = &vulkan12Features;
    vulkan12Features.pNext = &vulkan13Features;
    vulkan13Features.pNext = &asFeatures;
    asFeatures.pNext = &rtFeatures;
    rtFeatures.pNext = &atomicFloatFeatures;
    atomicFloatFeatures.pNext = &clockFeatures;

    return features2;
}

}