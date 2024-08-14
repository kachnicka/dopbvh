#include <format>
#include <string_view>
#include <vLime/Util.h>
#include <vLime/Vulkan.h>
#include <vulkan/vk_platform.h>
#include <vulkan/vulkan_core.h>

// #ifndef LIME_VULKAN_AS_NAMED_MODULE
// #include <vulkan/vulkan_handles.hpp>
// #endif

namespace lime::debug {

static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(
    // vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    // vk::DebugUtilsMessageTypeFlagsEXT messageType,
    // vk::DebugUtilsMessengerCallbackDataEXT const* pCallbackData,
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
    void* pUserData)
{
    static_cast<void>(messageSeverity);
    static_cast<void>(messageType);
    static_cast<void>(pUserData);

    // const auto severity = vk::to_string(vk::DebugUtilsMessageSeverityFlagBitsEXT(messageSeverity));
    // const auto type = vk::to_string(vk::DebugUtilsMessageTypeFlagsEXT(messageType));
    std::string_view const msg { pCallbackData->pMessage };

    // TODO: tmp suppression of validation errors that I am unable to fix right away
    using namespace std::literals;
    auto constexpr suppressed { "Suppressed validation error: "sv };
    std::array constexpr vErr {
        "VUID-VkShaderModuleCreateInfo-pCode-08737"sv,
        "VUID-VkPipelineShaderStageCreateInfo-pSpecializationInfo-06849"sv,
    };
    static std::array suppressedCount { 0, 0 };
    for (size_t i { 0 }; i < vErr.size(); ++i)
        if (msg.find(vErr[i]) != std::string_view::npos) {
            if (++suppressedCount[i] == 1)
                log::error(std::format("{} {}", suppressed, vErr[i]));
            return vk::False;
        }

    log::error(msg);
    return vk::False;
}

vk::ResultValue<vk::UniqueDebugUtilsMessengerEXT> CreateDebugMessenger(vk::Instance const i)
{
    vk::DebugUtilsMessengerCreateInfoEXT createInfo;
    // createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagsEXT(vk::FlagTraits<vk::DebugUtilsMessageSeverityFlagBitsEXT>::allFlags);
    createInfo.messageSeverity = vk::DebugUtilsMessageSeverityFlagsEXT(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning);
    createInfo.messageType = vk::DebugUtilsMessageTypeFlagsEXT(vk::FlagTraits<vk::DebugUtilsMessageTypeFlagBitsEXT>::allFlags);
    createInfo.pfnUserCallback = debugCallback;

    return i.createDebugUtilsMessengerEXTUnique(createInfo);
}

}
