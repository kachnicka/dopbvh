#include "SceneRenderer.h"

#include <cmath>
#include <imgui.h>

namespace module {

static f32 light_phi = glm::pi<f32>() + glm::half_pi<f32>() + glm::quarter_pi<f32>();
static f32 light_theta = glm::quarter_pi<f32>();
static f32 light_power = 1.f;
static glm::vec4 dirLight = {
    glm::sin(light_theta) * glm::cos(light_phi),
    glm::sin(light_theta) * glm::sin(light_phi),
    glm::cos(light_theta),
    light_power,
};

static std::array constexpr strRenderMode {
    "Path Tracing KHR",
    "Path Tracing compute",
    "Debug view",
};

static std::array constexpr strBvhBuilder {
    "PLOC++",
};

static std::array constexpr strMorton {
    "Morton30 (32bit)",
    "Morton60 (64bit)",
};

static std::array constexpr strVisMode {
    "Path tracing",
    "Bounding Volume",
    "BV intersections",
};

void SceneRenderer::ProcessGUI(double deltaTime_TMP)
{
    // TODO: why is this logic here?
    backend.state.oneSecondAcc += deltaTime_TMP;
    backend.state.oncePerSecond = false;
    if (backend.state.oneSecondAcc > 1.) {
        backend.state.oneSecondAcc -= 1.;
        backend.state.oncePerSecond = true;
    }
    backend.state.hundredMsAcc += deltaTime_TMP;
    backend.state.oncePerHundredMs = false;
    if (backend.state.hundredMsAcc > .1) {
        backend.state.hundredMsAcc -= .1;
        backend.state.oncePerHundredMs = true;
    }

    ImGui::Begin("Render window");
    if ([[maybe_unused]] auto const screenResized { window.ProcessGUI(reinterpret_cast<ImTextureID>(backend::gui::TextureID::renderedScene)) }) {
        berry::Log::debug("Resize: Render window {}x{}", window.camera.screenResolution.x, window.camera.screenResolution.y);
        backend.ResizeRenderer(window.camera.screenResolution.x, window.camera.screenResolution.y);
    }
    if (window.camera.UpdateCamera())
        backend.ResetAccumulation();

    auto const posAbsRenderWindow { ImGui::GetWindowPos() };
    auto const extentContent { ImGui::GetContentRegionAvail() };
    auto const posAbsMouse { ImGui::GetMousePos() };
    auto const posAbsButton { ImGui::GetItemRectMin() };

    glm::i32vec2 const posMouseRenderWindow { posAbsMouse.x - posAbsButton.x, posAbsMouse.y - posAbsButton.y };
    glm::i32vec2 const posRelMouse { posAbsMouse.x - posAbsRenderWindow.x, posAbsMouse.y - posAbsRenderWindow.y };
    glm::u32vec2 const extentUint { extentContent.x, extentContent.y };

    auto vMin { ImGui::GetWindowContentRegionMin() };
    auto vMax { ImGui::GetWindowContentRegionMax() };
    vMin.x += ImGui::GetWindowPos().x;
    vMin.y += ImGui::GetWindowPos().y;
    vMax.x += ImGui::GetWindowPos().x;
    vMax.y += ImGui::GetWindowPos().y;

    if (backend.IsRendering()) {
        ImGui::GetWindowDrawList()->AddRect(vMin, vMax, IM_COL32(255, 0, 0, 255));
    }

    ImGui::End();

    if (ImGui::IsKeyPressed(ImGuiKey_F)) {
        backend.state.selectedRenderMode_TMP = (backend.state.selectedRenderMode_TMP + 1) % static_cast<int>(strRenderMode.size());
        backend.ResetAccumulation();
    }

    ImGui::Begin("Scene renderer");

    ImGui::Text("Render resolution : %.0u, %.0u", window.camera.screenResolution.x, window.camera.screenResolution.y);
    ImGuiIO& io = ImGui::GetIO();
    if (ImGui::Checkbox("vSync", &backend.state.vSync))
        backend.setVSync(backend.state.vSync);
    ImGui::SameLine();
    ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
    if (ImGui::Combo("Render Mode", &backend.state.selectedRenderMode_TMP, strRenderMode.data(), static_cast<int>(strRenderMode.size()))) {
        backend.ResetAccumulation();
    }

    ImGui::Separator();
    switch (backend::vulkan::State::Renderer(backend.state.selectedRenderMode_TMP)) {
    case backend::vulkan::State::Renderer::ePathTracingKHR:
        guiRayTracerShared();
        ImGui::Separator();
        guiPathTracerKHR();
        break;
    case backend::vulkan::State::Renderer::ePathTracingCompute:
        guiRayTracerShared();
        ImGui::Separator();
        guiPathTracerCompute();
        break;
    case backend::vulkan::State::Renderer::eDebug:
        guiDebugView();
        break;
    }
    ImGui::Separator();

    ImGui::End();
}

void SceneRenderer::guiRayTracerShared()
{
    auto const sppButton { [this](u32 spp) {
        if (ImGui::Button(std::to_string(spp).c_str())) {
            backend.samplesPerPixel = spp;
            backend.ResetAccumulation();
        }
    } };
    ImGui::Text("Sample count: ");
    ImGui::SameLine();
    sppButton(1);
    ImGui::SameLine();
    sppButton(8);
    ImGui::SameLine();
    sppButton(128);
    ImGui::SameLine();
    if (ImGui::Button("inf")) {
        backend.samplesPerPixel = std::numeric_limits<u32>::max();
        backend.ResetAccumulation();
    }
    if (gui::u32Slider("##SampleCounterSlider", &backend.samplesPerPixel, 1, 2048, "%u", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic))
        backend.ResetAccumulation();
    ImGui::Text("Samples (%u): %u", backend.samplesPerPixel, backend.samplesComputed);

    ImGui::Separator();
    ImGui::Text("dir. light: %.2f %.2f %.2f %.2f", dirLight.x, dirLight.y, dirLight.z, dirLight.w);
    bool updateLight = false;
    if (ImGui::DragFloat("phi   (0..2pi)", &light_phi, .01f, 0.f, 2.f * glm::pi<f32>()))
        updateLight = true;
    if (ImGui::DragFloat("theta (0..pi)", &light_theta, .005f, 0.f, glm::pi<f32>()))
        updateLight = true;
    if (ImGui::DragFloat("power", &light_power, .005f, 0.f, 10.f, nullptr, ImGuiSliderFlags_Logarithmic))
        updateLight = true;

    if (updateLight) {
        auto const sinTheta = glm::sin(light_theta);
        dirLight.x = sinTheta * glm::cos(light_phi);
        dirLight.y = sinTheta * glm::sin(light_phi);
        dirLight.z = glm::cos(light_theta);
        dirLight.w = light_power;
        backend.ResetAccumulation();
    }
    if (backend.pt_KHR)
        backend.pt_KHR->renderer.dirLight_TMP = dirLight;
    if (backend.pt_compute && backend.pt_compute->GetConfig().tracer.traceMode != 2)
        memcpy(&backend.pt_compute->tracer.dirLight_TMP, &dirLight, 16);
}

void SceneRenderer::guiPathTracerKHR()
{
    static float mRaysPerSecond { 0.f };
    ImGui::Checkbox("Read ray counter", &backend.state.readBackRayCount);
    if (backend.state.readBackRayCount) {
        if (backend.state.oncePerSecond)
            mRaysPerSecond = backend.pt_KHR->renderer.RayCounterReadBackAndReset() / 1'000'000.f;
        ImGui::SameLine();
        ImGui::Text("  ~%.2f Mrps", mRaysPerSecond);
    }
}

void SceneRenderer::guiPathTracerCompute()
{
    ImGui::BeginDisabled();
    if (ImGui::TreeNodeEx("BVH construction")) {

        if (ImGui::Combo("BVH Builder", &backend.state.selectedBvhBuilder_TMP, strBvhBuilder.data(), csize<int>(strBvhBuilder))) {
            ;
        }
        switch (backend::vulkan::State::BvhBuilder(backend.state.selectedBvhBuilder_TMP)) {
        case backend::vulkan::State::BvhBuilder::ePLOCpp:
            guiPLOCpp();
            break;
        }
        ImGui::Separator();
        ImGui::TreePop();
    }
    ImGui::Separator();

    if (ImGui::TreeNodeEx("BVH traversal")) {
        // if (gui::u32Slider("Workgroup (32x6) count", &backend.pt_compute->renderer.traceWorkgroupsToStart_TMP, 1, 1024, "%u", ImGuiSliderFlags_AlwaysClamp)) {
        //     ;
        // }

        ImGui::TreePop();
    }
    ImGui::EndDisabled();
    ImGui::Separator();

    if (ImGui::TreeNodeEx("Output", ImGuiTreeNodeFlags_DefaultOpen)) {
        static u32 depthViz { 1 };
        static u32 depthIntersections_min { 250 };
        static u32 depthIntersections_max { 1048576 };
        bool setMode { false };
        static int selectedVizMode { std::to_underlying(backend::vulkan::bvh::Tracer::Mode::ePathTracing) };
        for (int i { 0 }; i < csize<int>(strVisMode); ++i) {
            if (ImGui::Selectable(strVisMode[i], selectedVizMode == i)) {
                selectedVizMode = i;
                setMode = true;
            }
        }
        ImGui::NewLine();
        auto cfg { backend.pt_compute->GetConfig() };
        switch (static_cast<backend::vulkan::bvh::Tracer::Mode>(selectedVizMode)) {
        case backend::vulkan::bvh::Tracer::Mode::ePathTracing:
            break;
        case backend::vulkan::bvh::Tracer::Mode::eBVVisualization:
            if (gui::u32Slider("##CurrentBVHLevel", &cfg.tracer.bvDepth, 0, 256, "%u", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic) || setMode)
                setMode = true;
            ImGui::SameLine();
            ImGui::BeginDisabled(cfg.tracer.bvDepth == 1);
            if (ImGui::Button("-")) {
                cfg.tracer.bvDepth--;
                setMode = true;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(depthViz == 256);
            if (ImGui::Button("+")) {
                cfg.tracer.bvDepth++;
                setMode = true;
            }
            ImGui::EndDisabled();
            break;
        case backend::vulkan::bvh::Tracer::Mode::eBVIntersections:
            if (gui::u32Slider("##CurrentLevelIntersections_min", &depthIntersections_min, 1, depthIntersections_max, "%u", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic) || setMode) {
                backend.pt_compute->tracer.dirLight_TMP[0] = static_cast<f32>(depthIntersections_min);
                setMode = true;
            }
            break;
        default:
            break;
        }
        if (setMode) {
            cfg.tracer.traceMode = selectedVizMode;
            backend.ResetAccumulation();
            backend.pt_compute->SetPipelineConfiguration(cfg);
            if (backend.pt_KHR)
                backend.pt_KHR->renderer.dirLight_TMP = dirLight;
            if (backend.pt_compute && backend.pt_compute->GetConfig().tracer.traceMode != 2)
                memcpy(&backend.pt_compute->tracer.dirLight_TMP, &dirLight, 16);
        }
        ImGui::TreePop();
    }
    ImGui::Separator();

    if (ImGui::TreeNodeEx("BVH stats", ImGuiTreeNodeFlags_DefaultOpen)) {
        static f32 pMRps { 0.f };
        static f32 sMRps { 0.f };
        static f32 buildTime { 0.f };

        static u64 pRayCount { 0 };
        static f32 pTraceTimeMs { 0.f };
        static u64 sRayCount { 0 };
        static f32 sTraceTimeMs { 0.f };

        static std::array<f32, 7> sMRps_perLevel {};
        static std::array<u64, 7> sRayCount_perLevel {};
        static std::array<f32, 7> sTraceTimeMs_perLevel {};

        auto const bStats { backend.pt_compute->GetStatsBuild() };
        auto const tStats { backend.pt_compute->GetStatsTrace() };
        pRayCount += tStats.data[0].rayCount;
        pTraceTimeMs += tStats.data[0].traceTimeMs;
        for (int i = 1; i < 8; i++) {
            sRayCount += tStats.data[i].rayCount;
            sTraceTimeMs += tStats.data[i].traceTimeMs;

            sRayCount_perLevel[i - 1] += tStats.data[i].rayCount;
            sTraceTimeMs_perLevel[i - 1] += tStats.data[i].traceTimeMs;
        }
        if (backend.state.oncePerHundredMs) {
            pMRps = (static_cast<f32>(pRayCount) * 1e-6f) / (pTraceTimeMs * 1e-3f);
            sMRps = (static_cast<f32>(sRayCount) * 1e-6f) / (sTraceTimeMs * 1e-3f);
            pRayCount = 0;
            pTraceTimeMs = 0.f;
            sRayCount = 0;
            sTraceTimeMs = 0.f;

            for (int i = 0; i < 7; i++) {
                sMRps_perLevel[i] = (static_cast<f32>(sRayCount_perLevel[i]) * 1e-6f) / (sTraceTimeMs_perLevel[i] * 1e-3f);
                sRayCount_perLevel[i] = 0;
                sTraceTimeMs_perLevel[i] = 0.f;
            }
        }
        buildTime = bStats.plocpp.timeTotal + bStats.collapsing.timeTotal + bStats.transformation.timeTotal + bStats.compression.timeTotal;

        ImGui::Text("    Primary:  ~%s Mrps", fmt::format("{:>8.2f}", pMRps).c_str());
        ImGui::Text("  Secondary:  ~%s Mrps", fmt::format("{:>8.2f}", sMRps).c_str());
        if (ImGui::TreeNodeEx("Secondary per bounce")) {
            for (int i = 0; i < 7; i++)
                ImGui::Text("     Sec. %d:  ~%s Mrps", i, fmt::format("{:>8.2f}", sMRps_perLevel[i]).c_str());
            ImGui::TreePop();
        }
        ImGui::Separator();
        ImGui::Text("  Full #Nodes:    %s", fmt::format("{:14L}", bStats.plocpp.nodeCountTotal).c_str());
        ImGui::Text("  Full area rel. i:      %s", fmt::format("{:>8.2f}", bStats.plocpp.saIntersect).c_str());
        ImGui::Text("  Full area rel. t:      %s", fmt::format("{:>8.2f}", bStats.plocpp.saTraverse).c_str());
        ImGui::Text("  Full SAH cost:        %s", fmt::format("{:>8.2f}", bStats.plocpp.costTotal).c_str());
        ImGui::Separator();
        ImGui::Text("  Collapsed #N:   %s", fmt::format("{:14L}", bStats.collapsing.nodeCountTotal).c_str());
        ImGui::Text("  Collapsed area rel. i: %s", fmt::format("{:>8.2f}", bStats.collapsing.saIntersect).c_str());
        ImGui::Text("  Collapsed area rel. t: %s", fmt::format("{:>8.2f}", bStats.collapsing.saTraverse).c_str());
        ImGui::Text("  Collapsed SAH cost:   %s", fmt::format("{:>8.2f}", bStats.collapsing.costTotal).c_str());
        if (bStats.transformation.costTotal > 0.f) {
            ImGui::Separator();
            ImGui::Text("  Transformed #N:   %s", fmt::format("{:14L}", bStats.transformation.nodeCountTotal).c_str());
            ImGui::Text("  Transformed area rel. i: %s", fmt::format("{:>8.2f}", bStats.transformation.saIntersect).c_str());
            ImGui::Text("  Transformed area rel. t: %s", fmt::format("{:>8.2f}", bStats.transformation.saTraverse).c_str());
            ImGui::Text("  Transformed SAH cost:   %s", fmt::format("{:>8.2f}", bStats.transformation.costTotal).c_str());
        }
        ImGui::Separator();
        ImGui::Text("  Compressed #N:   %s", fmt::format("{:14L}", bStats.compression.nodeCountTotal).c_str());
        ImGui::Text("  Compressed area rel. i: %s", fmt::format("{:>8.2f}", bStats.compression.saIntersect).c_str());
        ImGui::Text("  Compressed area rel. t: %s", fmt::format("{:>8.2f}", bStats.compression.saTraverse).c_str());
        ImGui::Text("  Compressed SAH cost:   %s", fmt::format("{:>8.2f}", bStats.compression.costTotal).c_str());
        ImGui::Separator();
        ImGui::Text("  Construction GPU: %s ms", fmt::format("{:>8.2f}", buildTime).c_str());

        ImGui::TreePop();
    }
    ImGui::Separator();
}

void SceneRenderer::guiDebugView()
{
    if (ImGui::Checkbox("Only selected", &backend.state.debugRenderWindowOnlySelected)) {
        backend.debugView->heatWireframe.renderOnlySelected = backend.state.debugRenderWindowOnlySelected;
    }
    if (ImGui::Checkbox("Draw scene", &backend.state.debugRenderWindowScene))
        backend.debugView->heatWireframe.renderScene = backend.state.debugRenderWindowScene;
    if (ImGui::Checkbox("AABB overlap heat map", &backend.state.debugRenderWindowHeatMap))
        backend.debugView->heatWireframe.renderHeatMap = backend.state.debugRenderWindowHeatMap;
    if (ImGui::Checkbox("AABB wireframe", &backend.state.debugRenderWindowWireframe))
        backend.debugView->heatWireframe.renderWireframe = backend.state.debugRenderWindowWireframe;
    ImGui::NewLine();
    ImGui::SliderFloat("Heat map scale", &backend.debugView->GetHeatMapScale_TMP(), 1.f, 256.f);
}

void SceneRenderer::guiPLOCpp()
{
    if (ImGui::TreeNodeEx("Morton codes")) {
        static int selected { std::to_underlying(backend::vulkan::State::MortonCode::eMorton30) };
        auto constexpr v0 { std::to_underlying(backend::vulkan::State::MortonCode::eMorton30) };
        auto constexpr v1 { std::to_underlying(backend::vulkan::State::MortonCode::eMorton60) };
        if (ImGui::Selectable(strMorton[v0], selected == v0))
            selected = v0;
        ImGui::BeginDisabled();
        if (ImGui::Selectable(strMorton[v1], selected == v1))
            selected = v1;
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted("Current radix sort implementation does not support 96bit keyval pairs (key Morton60: 64; val nodeId : 32)");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::EndDisabled();
        ImGui::TreePop();
    }

    if (ImGui::TreeNodeEx("PLOC radius", ImGuiTreeNodeFlags_DefaultOpen)) {
        auto const radiusButton { [this](u32 radius) {
            if (ImGui::Button(std::to_string(radius).c_str())) {
                plocRadius = radius;
                backend.ResetAccumulation();
            }
        } };
        radiusButton(8);
        ImGui::SameLine();
        radiusButton(16);
        ImGui::SameLine();
        radiusButton(32);
        ImGui::SameLine();
        radiusButton(64);
        ImGui::SameLine();
        radiusButton(100);
        ImGui::SameLine();
        radiusButton(128);
        ImGui::SameLine();
        ImGui::Text("%u", plocRadius);

        ImGui::TreePop();
    }
    if (ImGui::TreeNodeEx("Collapsing", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (gui::u32Slider("max leaf size", &collapsingThreshold, 2, 7, "%u", ImGuiSliderFlags_AlwaysClamp)) {
            ;
        }

        if (gui::u32Slider("c t", &backend.state.bvhCollapsing_c_t, 1, 10, "%u", ImGuiSliderFlags_AlwaysClamp)) {
            ;
        }
        if (gui::u32Slider("c i", &backend.state.bvhCollapsing_c_i, 1, 10, "%u", ImGuiSliderFlags_AlwaysClamp)) {
            ;
        }

        ImGui::TreePop();
    }
}

}
