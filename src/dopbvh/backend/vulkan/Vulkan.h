#pragma once

#include "../../core/Taskflow.h"
#include "MyCapabilities.h"
#include "RadixSort.h"
#include "Task.h"
#include "VCtx.h"
#include "VulkanState.h"
#include "data/DeviceData.h"
#include <berries/lib_helper/glfw.h>
#include <berries/lib_helper/spdlog.h>
#include <filesystem>
#include <vLime/ExtensionsAndLayers.h>
#include <vLime/Memory.h>
#include <vLime/Reflection.h>
#include <vLime/RenderGraph.h>
#include <vLime/SwapChain.h>
#include <vLime/Transfer.h>
#include <vLime/vLime.h>

namespace backend::vulkan {

class Vulkan {
private:
    lime::Capabilities capabilities;
    lime::Instance instance;
    lime::Device device;

    vk::Instance i;
    vk::PhysicalDevice pd;
    vk::Device d;

    lime::MemoryManager memory;
    lime::Transfer transfer;
    lime::ShaderCache sCache;
    lime::SwapChain swapChain;

    lime::Frame frame;
    lime::rg::Graph rg;
    lime::rg::id::Resource rgSwapChain;
    bool transitionSwapChain { true };

public:
    data::DeviceData deviceData;

    std::unique_ptr<task::PathTracerKHR> pt_KHR { nullptr };
    std::unique_ptr<task::PathTracerCompute> pt_compute { nullptr };
    std::unique_ptr<task::DebugView> debugView { nullptr };
    std::unique_ptr<task_old_style::ImGui> imgui { nullptr };

    std::vector<data::Scene> scenes;
    data::Scene* selectedScene { nullptr };
    State state;

    struct {
        Executor executor;
        Taskflow taskflow { "vulkan" };
        Task rgCompile;
        Task rgExecute;

        void reset()
        {
            rgCompile = {};
            rgExecute = {};
            taskflow.clear();
        }
    } schedule;

public:
    Vulkan(berry::Window const& window, std::filesystem::path const& shaders);

    VCtx Get_TMP()
    {
        return ctx();
    }

    void BeginFrame();
    void SubmitFrame();
    void WaitIdle() const;

    void InitGUIRenderer(char const* pathFont);
    task::PathTracerKHR* InitPathTracerKHR();
    task::DebugView* InitDebugView();
    task::PathTracerCompute* InitPathTracerCompute();

    data::ID_Texture AddTexture(u32 x, u32 y, char const* data, Format format = Format::eR8G8B8A8Unorm);
    data::ID_Geometry AddGeometry(input::Geometry const& g);

    void UnloadScene();

    void CompileRenderGraph();
    void Update();

    void Describe();
    void SetViewerMode();
    void setVSync(bool enable);
    void selectScene(u32 sceneIndex);
    void selectNode(u32 nodeIndex) const;

    void ResizeSwapChain();
    void ResizeRenderer(u32 x, u32 y) const;
    void recompileRenderGraph();

    // TODO: find better place for these
    static u32 constexpr SAMPLES_PER_FRAME_ACCUMULATION_TMP_ToBeReplacedByRuntimeEstimate { 8 };
    u32 samplesToComputeThisFrame { 1 };
    bool resetAccumulation { true };
    u32 samplesPerPixel { 1 };
    u32 samplesComputed { 0 };

    [[nodiscard]] bool IsRendering() const
    {
        return samplesToComputeThisFrame > 0;
    }

    void ResetAccumulation()
    {
        resetAccumulation = true;
    }

    void CalculateSampleCountsForFrame();

    void CleanUp_TMP() {
        FuchsiaRadixSort::RadixSortDestroy(d);
    }
private:
    static lime::Capabilities setupVulkanBackend()
    {
        lime::LoadVulkan();
        lime::log::SetCallbacks(&berry::Log::info, &berry::Log::debug, &berry::Log::error);

        lime::Capabilities capabilities;
        capabilities.add<BasicGraphics>();
        capabilities.add<RayTracing_KHR>();
        capabilities.add<RayTracing_compute>();
        capabilities.add<FuchsiaRadixSort>();
        capabilities.add<OnScreenPresentation>();

        return capabilities;
    }

    static lime::Device createDevice(vk::PhysicalDevice pd, lime::Capabilities& capabilities)
    {
        FuchsiaRadixSort::RadixSortInit(pd);
        return { pd, capabilities };
    }

    static lime::MemoryManager createMemoryManager(vk::Device d, vk::PhysicalDevice pd, lime::Capabilities& capabilities)
    {
        lime::MemoryManager memory(d, pd);
        auto const dFeatures { lime::device::CheckAndSetDeviceFeatures(capabilities, pd, true) };
        if (dFeatures.vulkan12Features.bufferDeviceAddress)
            memory.features.bufferDeviceAddress = true;
        return memory;
    }

    [[nodiscard]] VCtx ctx()
    {
        return { d, pd, memory, transfer, sCache };
    }

    static lime::Window limeWindow(berry::Window const& w)
    {
#ifdef VK_USE_PLATFORM_WIN32_KHR
        return { berry::Window::GetWin32ModuleHandle(), w.GetWin32Window() };
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
        return { w.GetX11Display(), w.GetX11Window() };
#endif
    }

    inline static vk::Format toVkFormat(Format format)
    {
        switch (format) {
        case Format::eR8G8B8A8Unorm:
            return vk::Format::eR8G8B8A8Unorm;
        case Format::eR32G32B32A32Sfloat:
            return vk::Format::eR32G32B32A32Sfloat;
        default:
            assert(0);
            return vk::Format::eUndefined;
        }
    }
};

}
