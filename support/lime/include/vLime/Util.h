#pragma once

#include <limits>
#include <string_view>
#include <vLime/Vulkan.h>
#include <vLime/types.h>

namespace lime {

namespace log {
void info(std::string_view const msg);
void debug(std::string_view const msg);
void error(std::string_view const msg);
void SetCallbacks(void (*callbackInfo)(std::string_view) = nullptr, void (*callbackDebug)(std::string_view) = nullptr, void (*callbackError)(std::string_view) = nullptr);
void SetCallbackDefaults();
}

inline constexpr u32 INVALID_ID { std::numeric_limits<u32>::max() };

[[nodiscard]] inline bool isValid(u32 const id)
{
    return id != INVALID_ID;
}

constexpr void check(vk::ResultValueType<void>::type vkResult)
{
    if (vkResult != vk::Result::eSuccess)
        abort();
}

[[nodiscard]] constexpr auto check(auto&& vkResult)
{
    switch (vkResult.result) {
    case vk::Result::eSuccess:
        return std::move(vkResult.value);
    case vk::Result::eErrorOutOfDeviceMemory:
        log::error("Allocation failed: out of device memory");
        return std::move(vkResult.value);
    case vk::Result::eErrorOutOfHostMemory:
        log::error("Allocation failed: out of host memory");
        return std::move(vkResult.value);
    default:
        abort();
    }
}

template<typename FlagBitType>
[[nodiscard]] constexpr bool checkVkFlags(vk::Flags<FlagBitType> flagsGiven, vk::Flags<FlagBitType> flagsToCheck) noexcept
{
    return (flagsToCheck & flagsGiven) == flagsToCheck;
}

template<typename FlagBitType>
[[nodiscard]] constexpr bool checkVkFlags(vk::Flags<FlagBitType> flagsGiven, FlagBitType flagToCheck) noexcept
{
    return (flagToCheck & flagsGiven) == flagToCheck;
}

[[nodiscard]] inline vk::UniqueFence FenceFactory(vk::Device d, vk::FenceCreateFlags const& flags = {})
{
    vk::FenceCreateInfo fenceCreateInfo;
    fenceCreateInfo.flags = flags;

    return check(d.createFenceUnique(fenceCreateInfo));
}

[[nodiscard]] inline vk::UniqueSemaphore SemaphoreFactory(vk::Device d, vk::SemaphoreCreateFlags const& flags = {}, bool exportSemaphore = false)
{
    vk::SemaphoreCreateInfo semaphoreCreateInfo;
    semaphoreCreateInfo.flags = flags;

    static_cast<void>(exportSemaphore);
    // vk::ExportSemaphoreCreateInfo exportSemaphoreInfo;
    //
    // if (exportSemaphore) {
    //     exportSemaphoreInfo.handleTypes |= ExternalHandleSupported::flagSemaphore;
    //     semaphoreCreateInfo.pNext = &exportSemaphoreInfo;
    // }
    return check(d.createSemaphoreUnique(semaphoreCreateInfo));
}

}
