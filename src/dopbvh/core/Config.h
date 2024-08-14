#pragma once

#include "../backend/Config.h"
#include <filesystem>
#include <string>
#include <vector>

class Config {
public:
    struct Scene {
        std::string name;
        std::string path;
        std::string camera;
        std::string bin;
    };

    static void ReadConfigFile(std::filesystem::path const&, std::filesystem::path const&);

    static std::vector<Config::Scene>& GetScenes();
    static Scene GetScene(std::string_view = {});

    static std::vector<backend::config::BVHPipeline> GetBVHPipelines(std::filesystem::path const& benchmark, std::vector<std::string>& scene, std::vector<std::string>& method);
    static std::vector<std::string> benchmark_scenes;
    static std::vector<std::string> benchmark_config;
};
