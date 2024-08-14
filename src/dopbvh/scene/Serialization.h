#pragma once

#include "Scene.h"
#include <filesystem>

namespace scene {

void serialize(Scene const& scene, std::filesystem::path const& dir);
Scene deserialize(std::filesystem::path const& path);

}