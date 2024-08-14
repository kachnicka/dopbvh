#pragma once

#include <iostream>
#include <GLFW/glfw3.h>
#include "spdlog.h"

#ifdef VK_USE_PLATFORM_WIN32_KHR
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <windows.h>
#endif

#ifdef VK_USE_PLATFORM_XLIB_KHR
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>
#include <X11/Xlib.h>
#endif

namespace berry
{

class Window
{
    static void myGlfwErrorCallback(int error, const char* description)
    {
        std::cerr << "GLFW Error '" << error << "': " << description << std::endl;
    }
    static void myGlfwKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        static_cast<void>(scancode);
        static_cast<void>(mods);

        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

public:
    GLFWwindow* window = nullptr;
    int width { 0 };
    int height { 0 };
public:
    Window(int width, int height, const char* title, GLFWkeyfun keyCallback = myGlfwKeyCallback)
        : width(width)
        , height(height)
    {
        glfwSetErrorCallback(myGlfwErrorCallback);
        if (!glfwInit())
            exit(1);

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(width, height, title, nullptr, nullptr);

        if (!window) {
            glfwTerminate();
            exit(0);
        }

        glfwSetKeyCallback(window, keyCallback);
        berry::log::timer("Window initialized.", glfwGetTime());
    }

    ~Window()
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    [[nodiscard]] bool ShouldClose() const
    {
        return glfwWindowShouldClose(window);
    }

    // [[nodiscard]] std::pair<uint32_t, uint32_t> GetFBSize()
    // {
    //     glfwGetFramebufferSize(window, &width, &height);
    //     return { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    // }

#ifdef VK_USE_PLATFORM_WIN32_KHR
    [[nodiscard]] static HINSTANCE GetWin32ModuleHandle()
    {
        return GetModuleHandle(nullptr);
    }
    [[nodiscard]] HWND GetWin32Window() const
    {
        return glfwGetWin32Window(window);
    }
#endif

#ifdef VK_USE_PLATFORM_XLIB_KHR
    Display* GetX11Display() const
    {
        return glfwGetX11Display();
    }
    ::Window GetX11Window() const
    {
        return glfwGetX11Window(window);
    }
#endif
};

}
