#pragma once

#include <string_view>
#include <vLime/Vulkan.h>
#include <vLime/types.h>
#include <vLime/Util.h>

namespace lime::debug {

#ifndef NDEBUG
inline constexpr bool ENABLE_DEBUG_UTILS { true };
#else
inline constexpr bool ENABLE_DEBUG_UTILS { false };
#endif

template<typename ObjType>
u64 GetVulkanHandle(ObjType cppHandle)
{
    return reinterpret_cast<uintptr_t>(static_cast<typename ObjType::CType>(cppHandle));
}

enum class LabelColor {
    eYellow,
    ePurple,
    eBordeaux,
    eGreen,
};

inline static std::array<f32, 4> getLabelColor(LabelColor c)
{
    switch (c) {
    case LabelColor::eYellow:
        return { .988f, .820f, .145f, 1.f };
    case LabelColor::ePurple:
        return { .369f, .067f, .859f, 1.f };
    case LabelColor::eBordeaux:
        return { .439f, .000f, .188f, 1.f };
    case LabelColor::eGreen:
        return { .377f, .969f, .106f, 1.f };
    }
    return { 1.f, 1.f, 1.f, 1.f };
}

template<typename>
struct always_false : std::false_type { };

template<typename ObjType>
static void SetObjectName(ObjType objHandle, char const* objName, vk::Device d)
{
    if constexpr (ENABLE_DEBUG_UTILS) {
        vk::DebugUtilsObjectNameInfoEXT info;

        if constexpr (std::is_same_v<ObjType, vk::Buffer>)
            info.objectType = vk::ObjectType::eBuffer;
        else if constexpr (std::is_same_v<ObjType, vk::Image>)
            info.objectType = vk::ObjectType::eImage;
        else if constexpr (std::is_same_v<ObjType, vk::ImageView>)
            info.objectType = vk::ObjectType::eImageView;
        else if constexpr (std::is_same_v<ObjType, vk::AccelerationStructureKHR>)
            info.objectType = vk::ObjectType::eAccelerationStructureKHR;
        else
            static_assert(always_false<ObjType>::value, "Debug name for this object type in not supported.");
        info.objectHandle = GetVulkanHandle(objHandle);
        info.pObjectName = objName;

        check(d.setDebugUtilsObjectNameEXT(info));
    }
}

inline static void BeginDebugLabel(vk::CommandBuffer const commandBuffer, char const* labelName, LabelColor const c = LabelColor::eYellow)
{
    if constexpr (ENABLE_DEBUG_UTILS) {
        vk::DebugUtilsLabelEXT label;
        label.pLabelName = labelName;
        label.color = getLabelColor(c);
        commandBuffer.beginDebugUtilsLabelEXT(label);
    }
}

inline static void EndDebugLabel(vk::CommandBuffer const commandBuffer)
{
    if constexpr (ENABLE_DEBUG_UTILS)
        commandBuffer.endDebugUtilsLabelEXT();
}

}
