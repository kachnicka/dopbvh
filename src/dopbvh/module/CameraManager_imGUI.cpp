#include "CameraManager.h"

#include <fstream>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

namespace module {

void CameraManager::ProcessGUI(State& state)
{
    static_cast<void>(state);
    ImGui::Begin("Camera manager");

    if (ImGui::Button("Add new view"))
        cameras.emplace_back(camera);
    ImGui::SameLine();
    ImGui::BeginDisabled(selectedCameraId == 99999);
    if (ImGui::Button("Replace view"))
        cameras[selectedCameraId] = camera;
    ImGui::SameLine();
    if (ImGui::Button("Delete view"))
        cameras.erase(cameras.begin() + selectedCameraId);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Reset view"))
        camera = Camera {};

    if (selectedCameraId >= static_cast<uint32_t>(cameras.size()))
        selectedCameraId = 99999;

    ImGui::InputText("File", &cameraCachePath);
    ImGui::SameLine();

    ImGui::BeginDisabled(cameras.empty());
    if (ImGui::Button("Save")) {
        std::ofstream outFile(cameraCachePath);
        for (auto& c : cameras)
            outFile << c;
        outFile.close();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        LoadCameraFile(cameraCachePath);
    }

    for (uint32_t i { 0 }; i < static_cast<uint32_t>(cameras.size()); i++) {
        if (ImGui::Selectable(std::to_string(i).c_str(), selectedCameraId == i, ImGuiSelectableFlags_AllowDoubleClick)) {
            selectedCameraId = i;
            if (ImGui::IsMouseDoubleClicked(0)) {
                camera = cameras[selectedCameraId];
                camera.changed = true;
            }
        }
    }

    ImGui::InputFloat("Pan speed factor", &camera.panSpeedFactor);
    ImGui::InputFloat("Zoom speed factor", &camera.zoomSpeedFactor);
    ImGui::InputFloat("Zoom limit", &camera.zoomLimit);
    ImGui::Text("Current camera:");
    ImGui::Text("  Orientation quaternion:");
    ImGui::Text("            %.2f %.2f %.2f %.2f", camera.orientation.w, camera.orientation.x, camera.orientation.y, camera.orientation.z);
    ImGui::Text("  Pivot   : %.2f %.2f %.2f", camera.pivot.x, camera.pivot.y, camera.pivot.z);
    auto const p { glm::vec3(mat4_cast(camera.orientation) * glm::vec4(camera.position, 1.f)) + camera.pivot };
    auto const u { glm::vec3(mat4_cast(camera.orientation) * glm::vec4(camera.up, 0.f)) };
    ImGui::Text("  WorldPos: %.2f %.2f %.2f", p.x, p.y, p.z);
    ImGui::Text("  Up vec  : %.2f %.2f %.2f.", u.x, u.y, u.z);

    auto& selectedCamera { selectedCameraId == 99999 ? camera : cameras[selectedCameraId] };
    ImGui::Text("Selected camera:");
    ImGui::Text("  Orientation quaternion:");
    ImGui::Text("            %.2f %.2f %.2f %.2f", selectedCamera.orientation.w, selectedCamera.orientation.x, selectedCamera.orientation.y, selectedCamera.orientation.z);
    ImGui::Text("  Pivot   : %.2f %.2f %.2f", selectedCamera.pivot.x, selectedCamera.pivot.y, selectedCamera.pivot.z);
    auto const ps { glm::vec3(mat4_cast(selectedCamera.orientation) * glm::vec4(selectedCamera.position, 1.f)) + selectedCamera.pivot };
    auto const us { glm::vec3(mat4_cast(selectedCamera.orientation) * glm::vec4(selectedCamera.up, 0.f)) };
    ImGui::Text("  WorldPos: %.2f %.2f %.2f", ps.x, ps.y, ps.z);
    ImGui::Text("  Up vec  : %.2f %.2f %.2f.", us.x, us.y, us.z);

    ImGui::End();
}

}
