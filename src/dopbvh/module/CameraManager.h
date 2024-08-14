#pragma once

#include "../ApplicationState.h"
#include "../scene/Camera.h"

namespace module {

class CameraManager {
    uint32_t selectedCameraId { 99999 };
    std::vector<Camera> cameras;
    std::string cameraCachePath { (std::filesystem::temp_directory_path() / "dopbvh_camera_cache_0").generic_string() };

public:
    Camera& camera;

    explicit CameraManager(Camera& camera)
        : camera(camera)
    {
    }

    void ProcessGUI(State& state);

    void LoadCameraFile(std::string_view path);

    void SetActiveCamera(uint32_t id)
    {
        if (id >= cameras.size())
            return;
        selectedCameraId = id;
        camera = cameras[id];
    }

    [[nodiscard]] std::vector<Camera> const& GetCameras() const
    {
        return cameras;
    }

    void ClearCameras()
    {
        selectedCameraId = 99999;
        cameras.clear();
    }
};

}
