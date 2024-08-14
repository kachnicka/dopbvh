#pragma once

#include "../backend/vulkan/Vulkan.h"
#include "../core/GUI.h"
#include "../scene/Camera.h"

namespace module {

class SceneGraph;

class SceneRenderer {
    uint32_t plocRadius { 16 };
    uint32_t collapsingThreshold { 7 };

    void guiRayTracerShared();
    void guiPathTracerKHR();
    void guiPathTracerCompute();
    void guiDebugView();

    void guiPLOCpp();

public:
    backend::vulkan::Vulkan& backend;
    gui::RendererIOWidget window;

public:
    explicit SceneRenderer(backend::vulkan::Vulkan& backend)
        : backend(backend)
    {
    }

    void ProcessGUI(double);

    void RenderFrame()
    {
        backend.CalculateSampleCountsForFrame();
        backend.deviceData.SetCamera_TMP(window.camera.GetMatrices());
    }

    Camera& GetCamera()
    {
        return window.camera;
    }
};

}
