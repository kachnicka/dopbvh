#pragma once

#include <memory>
#include <vector>

#include "ApplicationState.h"
#include "core/Config.h"
#include "core/Taskflow.h"
#include "module/Benchmark.h"
#include "module/CameraManager.h"
#include "module/SceneRenderer.h"
#include "scene/Scene.h"
#include <berries/lib_helper/glfw.h>

class Application {
public:
    struct RuntimeDirectory {
        std::filesystem::path bin;
        std::filesystem::path cwd;
        std::filesystem::path res;

        RuntimeDirectory(int argc, char* argv[]);

    } directory;

    berry::Window window;

    backend::vulkan::Vulkan backend;
    module::SceneRenderer sceneRenderer;
    module::CameraManager cameraManager;
    module::Benchmark benchmark;

    bool toggleMode_SHOULD_BE_REPLACED_BY_TASK { false };

    Executor mainExecutor;
    Taskflow perFrameTasks;

    struct {
        Task renderFrame;
    } task;

    struct AsyncProcessing {
        Application& app;
        explicit AsyncProcessing(Application& app)
            : app(app)
        {
        }

        std::vector<Future<std::optional<Scene>>> scenes;

        void checkState();
    } asyncProcessing { *this };

    std::vector<std::unique_ptr<Scene>> scenes;

    std::vector<std::string> availableDevices;
    State state;

    void saveImage_TMP(std::string);

public:
    explicit Application(int argc, char* argv[]);
    int Run();
    void Exit();

    void ProcessGUI();

    void toggleAppMode();

    void loadScene(Config::Scene const& scene);
    void loadSceneAssimp(std::string_view path);
    void loadSceneBinary(std::string_view path);
    void unloadScene();

    void setApplicationMode(State::AppMode mode);
};
