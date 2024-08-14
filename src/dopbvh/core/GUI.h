#pragma once

#include "../scene/Camera.h"
#include <backends/imgui_impl_glfw.h>
#include <berries/lib_helper/glfw.h>
#include <glm/ext/vector_int2_sized.hpp>
#include <imgui.h>

namespace gui {

inline static void create(berry::Window const& w)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    //        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
    //        io.ConfigViewportsNoAutoMerge = true;
    //        io.ConfigViewportsNoTaskBarIcon = true;

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }
    ImGui_ImplGlfw_InitForVulkan(w.window, true);
}

inline static void destroy()
{
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

inline static void new_frame()
{
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

inline static void end_frame()
{
    ImGui::EndFrame();
}

struct RendererIOWidget {
    Camera camera {};

    bool ProcessGUI(ImTextureID texId)
    {
        auto const extentContent { ImGui::GetContentRegionAvail() };
        auto const posAbsRenderWindow { ImGui::GetWindowPos() };
        auto const posAbsMouse { ImGui::GetMousePos() };
        glm::i32vec2 const posRelMouse { posAbsMouse.x - posAbsRenderWindow.x, posAbsMouse.y - posAbsRenderWindow.y };
        glm::u32vec2 const extentUint { extentContent.x, extentContent.y };

        // read mouse position
        camera.xPosCurrent = posAbsMouse.x - posAbsRenderWindow.x;
        camera.yPosCurrent = posAbsMouse.y - posAbsRenderWindow.y;

        ImGui::ImageButton(texId, extentContent, { 0, 0 }, { 1, 1 }, 0, ImVec4(0.f, 0.f, 0.f, 1.f));
        auto const posAbsButton { ImGui::GetItemRectMin() };

        glm::i32vec2 const posMouseRenderWindow { posAbsMouse.x - posAbsButton.x, posAbsMouse.y - posAbsButton.y };

        auto& io { ImGui::GetIO() };
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
            if (io.KeyShift)
                camera.PanStart();
            else
                camera.RotationStart();
        } else if (ImGui::IsMouseReleased(ImGuiMouseButton_Middle))
            camera.SetIdleState();

        if (ImGui::IsItemHovered() && io.MouseWheel != 0.0f)
            camera.Zoom(io.MouseWheel);

        return camera.ScreenResize(extentUint.x, extentUint.y);
    }
};

inline static void PrintMat4x4ColumnMajor(float const* v)
{
    ImGui::Text("%.2f %.2f %.2f %.2f", v[0], v[4], v[8], v[12]);
    ImGui::Text("%.2f %.2f %.2f %.2f", v[1], v[5], v[9], v[13]);
    ImGui::Text("%.2f %.2f %.2f %.2f", v[2], v[6], v[10], v[14]);
    ImGui::Text("%.2f %.2f %.2f %.2f", v[3], v[7], v[11], v[15]);
}

inline static bool u32Slider(char const* label, uint32_t* v, uint32_t v_min, uint32_t v_max, char const* format, ImGuiSliderFlags flags)
{
    return ImGui::SliderScalar(label, ImGuiDataType_U32, v, &v_min, &v_max, format, flags);
}

};
