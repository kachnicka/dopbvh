#pragma once

#include <vLime/vLime.h>

#ifdef VK_USE_PLATFORM_WIN32_KHR
#    include <windows.h>

namespace lime {
struct Window {
    HINSTANCE hinstance;
    HWND hwnd;
};
}
#endif

#ifdef VK_USE_PLATFORM_XLIB_KHR
#    include <X11/Xlib.h>

namespace lime {
struct Window {
    Display* dpy;
    ::Window window;
};
}
#endif

namespace lime {
static inline vk::UniqueSurfaceKHR getUniqueVulkanSurface(vk::Instance i, Window const& w)
{
#ifdef VK_USE_PLATFORM_WIN32_KHR
    vk::Win32SurfaceCreateInfoKHR cInfo { .hinstance = w.hinstance, .hwnd = w.hwnd };
    return lime::check(i.createWin32SurfaceKHRUnique(cInfo));
#endif // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_XLIB_KHR
    vk::XlibSurfaceCreateInfoKHR cInfo { .dpy = w.dpy, .window = w.window };
    return lime::check(i.createXlibSurfaceKHRUnique(cInfo));
#endif // VK_USE_PLATFORM_XLIB_KHR
}

static inline vk::ExternalMemoryHandleTypeFlagBits getExternalMemoryHandleTypeFlags()
{
#ifdef VK_USE_PLATFORM_WIN32_KHR
    return vk::ExternalMemoryHandleTypeFlagBits::eOpaqueWin32;
#endif // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_XLIB_KHR
    return vk::ExternalMemoryHandleTypeFlagBits::eOpaqueFd;
#endif // VK_USE_PLATFORM_XLIB_KHR
}

static inline intptr_t getExternalMemoryHandle(vk::Device d, vk::DeviceMemory memory)
{
#ifdef VK_USE_PLATFORM_WIN32_KHR
    vk::MemoryGetWin32HandleInfoKHR handleInfo { .memory = memory, .handleType = getExternalMemoryHandleTypeFlags() };
    return reinterpret_cast<intptr_t>(lime::check(d.getMemoryWin32HandleKHR(handleInfo)));
#endif // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_XLIB_KHR
    vk::MemoryGetFdInfoKHR handleInfo { .memory = memory, .handleType = getExternalMemoryHandleTypeFlags() };
    return static_cast<intptr_t>(lime::check(d.getMemoryFdKHR(handleInfo)));
#endif // VK_USE_PLATFORM_XLIB_KHR
}

#ifdef VK_USE_PLATFORM_WIN32_KHR
static inline vk::ImportMemoryWin32HandleInfoKHR getImportMemoryInfo(intptr_t externalHandle)
{
    return { .handleType = getExternalMemoryHandleTypeFlags(), .handle = reinterpret_cast<void*>(externalHandle) };
#endif // VK_USE_PLATFORM_WIN32_KHR
#ifdef VK_USE_PLATFORM_XLIB_KHR
    static inline vk::ImportMemoryFdInfoKHR getImportMemoryInfo(intptr_t externalHandle)
    {
        return { .handleType = getExternalMemoryHandleTypeFlags(), .fd = static_cast<int>(externalHandle) };
#endif // VK_USE_PLATFORM_XLIB_KHR
    }
}
