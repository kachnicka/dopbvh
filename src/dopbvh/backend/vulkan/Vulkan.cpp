#include "Vulkan.h"

#include "MyCapabilities.h"

namespace backend::vulkan {

Vulkan::Vulkan(berry::Window const& window, std::filesystem::path const& shaders)
    : capabilities(setupVulkanBackend())
    , instance(capabilities)
    , device(createDevice(lime::ListAllPhysicalDevicesInGroups(instance.get())[0][0], capabilities))
    , i(instance.get())
    , pd(device.getPd())
    , d(device.get())
    , memory(createMemoryManager(d, pd, capabilities))
    , transfer(device.queues.transfer, memory)
    , sCache(d, shaders)
    , swapChain(i, d, pd, limeWindow(window))
    , frame(d, device.queues.graphics)
    , rg(d, memory)
    , deviceData(ctx())
{
    FuchsiaRadixSort::RadixSortCreate(d, sCache.getPipelineCache());

    InitPathTracerKHR();
    InitPathTracerCompute();
    InitDebugView();
}

void Vulkan::BeginFrame()
{
    schedule.reset();

    schedule.rgExecute = schedule.taskflow.emplace([this]() {
        auto const [frameId, commandBuffer] { frame.ResetAndBeginCommandsRecording() };
        auto const backBufferDetail { swapChain.GetNextImage(frame.imageAvailableSemaphore.get()) };
        bool const haveImageToPresent { backBufferDetail.image };

        if (haveImageToPresent) {
            if (selectedScene) {
                switch (static_cast<State::Renderer>(state.selectedRenderMode_TMP)) {
                case State::Renderer::ePathTracingKHR:
                    if (pt_KHR) {
                        pt_KHR->RenderFrame(commandBuffer, deviceData, *selectedScene);
                        imgui->scene.textureRenderedScene = pt_KHR->rg.GetManagedBackingResource(pt_KHR->ptImg).getView();
                    }
                    break;
                case State::Renderer::ePathTracingCompute:
                    if (pt_compute) {
                        pt_compute->RenderFrame(commandBuffer, deviceData, *selectedScene);
                        imgui->scene.textureRenderedScene = pt_compute->rg.GetManagedBackingResource(pt_compute->ptImg).getView();
                    }
                    break;
                case State::Renderer::eDebug:
                    debugView->RenderFrame(commandBuffer, deviceData, *selectedScene);
                    imgui->scene.textureRenderedScene = debugView->rg.GetManagedBackingResource(debugView->imgHeatWireframe).getView();
                    break;
                }
            }

            rg.SetupExecution(commandBuffer, swapChain.imageIndexToPresentNext);
        }

        frame.EndCommandsRecording();
        frame.SubmitCommands(true, haveImageToPresent);
    });
}

void Vulkan::SubmitFrame()
{
    CompileRenderGraph();

    if (!schedule.rgCompile.empty())
        schedule.rgCompile.precede(schedule.rgExecute);

    schedule.executor.run(schedule.taskflow);
    schedule.executor.wait_for_all();

    swapChain.Present(device.queues.graphics.q, { frame.renderFinishedSemaphore.get() });
    frame.Wait();

    // TODO: testbed, internally tracks to rebuild once
    if (State::Renderer(state.selectedRenderMode_TMP) == State::Renderer::ePathTracingCompute && pt_compute && selectedScene) {
        if (pt_compute->BVHBuildPiecewise(*selectedScene))
            resetAccumulation = true;
    }
}

void Vulkan::InitGUIRenderer(char const* pathFont)
{
    imgui = std::make_unique<task_old_style::ImGui>(ctx());
    imgui->scene.SetDefaultTexture(deviceData.textures.GetDefaultTextureImageView());
    imgui->scene.upload_font_texture(deviceData.textures, pathFont);
}

task::PathTracerKHR* Vulkan::InitPathTracerKHR()
{
    if (!capabilities.isAvailable<RayTracing_KHR>())
        return nullptr;

    pt_KHR = std::make_unique<task::PathTracerKHR>(ctx(), device.queues.graphics);
    return pt_KHR.get();
}

task::PathTracerCompute* Vulkan::InitPathTracerCompute()
{
    if (!capabilities.isAvailable<RayTracing_compute>())
        return nullptr;

    pt_compute = std::make_unique<task::PathTracerCompute>(ctx(), device.queues.graphics);
    return pt_compute.get();
}

task::DebugView* Vulkan::InitDebugView()
{
    debugView = std::make_unique<task::DebugView>(ctx(), device.preferredDepthFormat_TMP);
    return debugView.get();
}

data::ID_Texture Vulkan::AddTexture(uint32_t x, uint32_t y, char const* data, Format format)
{
    return deviceData.textures.add(x, y, data, toVkFormat(format));
}

data::ID_Geometry Vulkan::AddGeometry(input::Geometry const& g)
{
    return deviceData.geometries.add(g);
}

void Vulkan::CompileRenderGraph()
{
    rg.reset();

    // TODO: this should probably not happen in this fashion after rg rework
    frame.ResetFrameCounter();

    rgSwapChain = rg.AddResource();
    rg.GetResource(rgSwapChain).BindToPhysicalResource(swapChain.GetAllImages());

    if (transitionSwapChain) {
        swapChain.scheduleTransitionToRenderGraph(rg, vk::ImageLayout::ePresentSrcKHR);
        transitionSwapChain = false;
    }

    // TODO: this need to manually reset previously bound resources is bad
    imgui->imgRendered = {};
    imgui->imgTarget = rgSwapChain;

    rg.GetResource(imgui->imgTarget).finalLayout = vk::ImageLayout::ePresentSrcKHR;
    imgui->scheduleToRenderGraph(rg);
    rg.GetResource(imgui->imgRendered).BindToPhysicalResource(deviceData.textures.GetDefaultTextureDetail());

    rg.Compile();
}

// TODO: update tasks should be spawned as a result of RG compilation, which determines
//       resources necessary for next frame
void Vulkan::Update()
{
}

// TODO: in this stage of development, descriptors are updated each frame,
//       as the dirty tracking system is not in place
void Vulkan::Describe()
{
}

void Vulkan::SetViewerMode()
{
    if (imgui) {
        // TODO: font texture should be released automatically with imgui scene data
        deviceData.textures.remove(imgui->scene.fontId);
        imgui.reset();
    }
}

void Vulkan::setVSync(bool enable)
{
    swapChain.vSync = enable;
    WaitIdle();
    swapChain.Resize();
    transitionSwapChain = true;
}

void Vulkan::selectScene(uint32_t sceneIndex)
{
    if (scenes.empty())
        return;

    selectedScene = &scenes[sceneIndex];

    if (pt_KHR)
        pt_KHR->RebuildAS();
}

void Vulkan::selectNode(uint32_t nodeIndex) const
{
    if (debugView)
        debugView->heatWireframe.selectedNode = nodeIndex;
}

void Vulkan::ResizeSwapChain()
{
    lime::check(device.queues.graphics.q.waitIdle());
    swapChain.Resize();
    transitionSwapChain = true;
}

void Vulkan::ResizeRenderer(u32 x, u32 y) const
{
    if (pt_KHR)
        pt_KHR->RecompileRenderGraph(x, y);
    if (pt_compute)
        pt_compute->RecompileRenderGraph(x, y);
    if (debugView)
        debugView->RecompileRenderGraph(x, y);
}

void Vulkan::recompileRenderGraph()
{
    if (schedule.rgCompile.empty())
        schedule.rgCompile = schedule.taskflow.emplace([this]() { CompileRenderGraph(); });
}

void Vulkan::WaitIdle() const
{
    device.WaitIdle();
}

void Vulkan::CalculateSampleCountsForFrame()
{
    if (!pt_KHR && !pt_compute)
        return;

    if (resetAccumulation) {
        samplesComputed = 0;
        u32 samplesPerFrame { 1 };
        if (State::Renderer::ePathTracingKHR == static_cast<State::Renderer>(state.selectedRenderMode_TMP))
            samplesPerFrame = SAMPLES_PER_FRAME_ACCUMULATION_TMP_ToBeReplacedByRuntimeEstimate;
        samplesToComputeThisFrame = std::min(samplesPerPixel, samplesPerFrame);
        resetAccumulation = false;
    } else {
        samplesComputed += samplesToComputeThisFrame;
        samplesToComputeThisFrame = std::min(samplesPerPixel - samplesComputed, samplesToComputeThisFrame);
    }
    if (pt_compute) {
        pt_compute->traceRuntimeData.samples.computed = samplesComputed;
        pt_compute->traceRuntimeData.samples.toCompute = samplesToComputeThisFrame;
    }
    if (pt_KHR) {
        pt_KHR->ptSamples_TMP.computed = samplesComputed;
        pt_KHR->ptSamples_TMP.toCompute = samplesToComputeThisFrame;
    }
}

void Vulkan::UnloadScene()
{
    scenes.clear();
    deviceData.reset();

    pt_KHR.reset();
    pt_compute.reset();
    debugView.reset();

    InitPathTracerKHR();
    InitPathTracerCompute();
    InitDebugView();

    selectedScene = nullptr;
    imgui->scene.textureRenderedScene = deviceData.textures.GetDefaultTextureImageView();
    memory.cleanUp();
}

}
