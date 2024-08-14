#include "CameraManager.h"

#include <filesystem>
#include <fstream>

namespace module {

void CameraManager::LoadCameraFile(std::string_view path)
{
    if (!std::filesystem::exists(path))
        return;

    cameraCachePath = path;

    std::ifstream inFile(path.data());
    cameras.clear();
    Camera c;
    while (inFile >> c)
        cameras.emplace_back(c);
    inFile.close();

    if (!cameras.empty())
        camera = cameras.front();
}

}