#include "Application.h"

#include "core/GUI.h"
#include "scene/SceneIO.h"
#include "util/pexec.h"

#include <berries/lib_helper/spdlog.h>
#include <filesystem>

#include "image/Writer.h"
#include "scene/Serialization.h"

namespace  dopbvh_glfw {

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    static_cast<void>(scancode);
    static_cast<void>(mods);

    auto* app { static_cast<Application*>(glfwGetWindowUserPointer(window)) };

    switch (key) {
    case GLFW_KEY_ESCAPE:
        if (action == GLFW_PRESS)
            app->Exit();
        break;
    case GLFW_KEY_F:
        if (action == GLFW_PRESS)
            app->toggleAppMode();
        break;
    default:;
    }
}

static void framebufferSizeCallback(GLFWwindow* window, int width, int height)
{
    auto* app { static_cast<Application*>(glfwGetWindowUserPointer(window)) };
    berry::Log::debug("Resize: Main window, {}x{} to {}x{}", app->window.width, app->window.height, width, height);
    app->window.width = width;
    app->window.height = height;
    app->backend.ResizeSwapChain();
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    static_cast<void>(window);
    static_cast<void>(mods);
    static_cast<void>(button);
    static_cast<void>(action);
}

static void scrollCallback(GLFWwindow* window, double xOffset, double yOffset)
{
    static_cast<void>(window);
    static_cast<void>(xOffset);
    static_cast<void>(yOffset);
}

}

Application::RuntimeDirectory::RuntimeDirectory(int argc, char* argv[])
    : bin(pexec::get_path_to_executable().parent_path())
    , cwd(std::filesystem::current_path())
{
    static_cast<void>(argc);
    static_cast<void>(argv);

    // locate resource dir
    auto path_to_explore { bin };
    auto const root { bin.root_directory() };

    if (res = bin / "data"; std::filesystem::exists(res))
        goto log_dirs;
    if (res = cwd / "data"; std::filesystem::exists(res))
        goto log_dirs;
    while (path_to_explore != root) {
        path_to_explore = path_to_explore.parent_path();
        if (res = path_to_explore / "data"; std::filesystem::exists(res))
            goto log_dirs;
    }
log_dirs:
    berry::Log::info("bin: {}", bin.generic_string());
    berry::Log::info("cwd: {}", cwd.generic_string());
    berry::Log::info("res: {}", res.generic_string());

    if (path_to_explore == root) {
        berry::Log::error("Can't locate '/data' folder.");
        exit(EXIT_FAILURE);
    }
}

static constexpr int WIDTH { 1920 };
static constexpr int HEIGHT { 1080 };
constexpr char const* defaultConfigFile { "scene.toml" };

Application::Application(int argc, char* argv[])
    : directory(argc, argv)
    , window(WIDTH, HEIGHT, "DopBVH", dopbvh_glfw::keyCallback)
    , backend(window, directory.res / "shaders")
    , sceneRenderer(backend)
    , cameraManager(sceneRenderer.GetCamera())
    , benchmark(*this)
{
    glfwSetFramebufferSizeCallback(window.window, dopbvh_glfw::framebufferSizeCallback);
    glfwSetWindowUserPointer(window.window, this);

    auto const& configFile { argc > 1 ? std::filesystem::path(argv[1]) : directory.res / defaultConfigFile };
    Config::ReadConfigFile(configFile, directory.res);
    state.sceneToLoad = Config::GetScene();

    setApplicationMode(State::AppMode::eEditor);
}

int Application::Run()
{
    double prevTime { 0. };
    while (!window.ShouldClose()) {
        if (!state.sceneToLoad.name.empty()) {
            unloadScene();
            loadScene(state.sceneToLoad);
            state.sceneToLoad = {};
        }
        benchmark.Run();

        backend.BeginFrame();
        glfwPollEvents();

        state.time = glfwGetTime();
        state.deltaTime = glfwGetTime() - prevTime;
        prevTime = glfwGetTime();
        asyncProcessing.checkState();

        gui::new_frame();
        ProcessGUI();
        cameraManager.ProcessGUI(state);
        sceneRenderer.ProcessGUI(state.deltaTime);
        benchmark.ProcessGUI(state);
        gui::end_frame();

        if (!scenes.empty())
            RecomputeMatrices(*scenes.back().get(), backend);

        sceneRenderer.RenderFrame();
        backend.SubmitFrame();
        state.frameId++;
    }

    backend.WaitIdle();
    backend.CleanUp_TMP();
    return EXIT_SUCCESS;
}

void Application::Exit()
{
    if (State::FAST_EXIT)
        exit(EXIT_SUCCESS);
    glfwSetWindowShouldClose(window.window, GLFW_TRUE);
}

void Application::toggleAppMode()
{
    toggleMode_SHOULD_BE_REPLACED_BY_TASK = true;
}

void Application::AsyncProcessing::checkState()
{
    constexpr auto isReady {
        [](auto const& f) {
            return f.wait_for(std::chrono::nanoseconds(1)) == std::future_status::ready;
        }
    };

    std::vector<Future<std::optional<Scene>>> notReady;
    for (auto& s : scenes) {
        if (isReady(s)) {
            app.scenes.emplace_back(std::make_unique<Scene>(std::move(s.get().value())));
            berry::log::timer("Scene load finished", glfwGetTime());
            app.state.sceneLoading = false;
        } else
            notReady.emplace_back(std::move(s));
    }
    scenes = std::move(notReady);
}

void Application::loadScene(Config::Scene const& scene)
{
    state.sceneLoading = true;
    state.statusBarFade = 1.0f;
    if (!scene.bin.empty())
        loadSceneBinary(scene.bin);
    else
        loadSceneAssimp(scene.path);
    cameraManager.LoadCameraFile(scene.camera);
    benchmark.SetScene(scene.name);
}

void Application::loadSceneBinary(std::string_view path)
{
    if (!std::filesystem::exists(path)) {
        berry::Log::error("File does not exist: {}", path);
        return;
    }
    berry::log::timer("Scene load started (binary)", glfwGetTime());
    asyncProcessing.scenes.push_back(mainExecutor.async([this, path = std::filesystem::path(path)]() {
        Scene result { scene::deserialize(path) };
        berry::log::timer("  deserialized", glfwGetTime());
        UploadScene(result, backend);
        berry::log::timer("  uploaded to GPU", glfwGetTime());

        // TODO: not thread safe!!
        backend.selectScene(0);
        backend.ResetAccumulation();

        result.path = path;
        return result;
    }));
}

void Application::loadSceneAssimp(std::string_view path)
{
    if (!std::filesystem::exists(path)) {
        berry::Log::error("File does not exist: {}", path);
        return;
    }
    berry::log::timer("Scene load started", glfwGetTime());
    asyncProcessing.scenes.push_back(mainExecutor.async([this, path = std::filesystem::path(path)]() {
        Scene result;
        SceneIO io;

        berry::log::timer("  importing..", glfwGetTime());
        io.ImportScene(path.generic_string(), &state.progress);
        berry::log::timer("  imported", glfwGetTime());
        result = io.CreateScene();
        berry::log::timer("  created", glfwGetTime());
        UploadScene(result, backend);
        berry::log::timer("  uploaded to GPU", glfwGetTime());

        // TODO: not thread safe!!
        backend.selectScene(0);
        backend.ResetAccumulation();

        result.path = path;
        return result;
    }));
}

void Application::unloadScene()
{
    if (scenes.empty())
        return;

    backend.UnloadScene();
    backend.ResizeRenderer(sceneRenderer.window.camera.screenResolution.x, sceneRenderer.window.camera.screenResolution.y);
    benchmark.SetDefaultAsActive();

    scenes.clear();
    cameraManager.ClearCameras();
}

void Application::setApplicationMode(State::AppMode mode)
{
    if (state.appMode == mode)
        return;

    switch (state.appMode) {
    case State::AppMode::eViewer:
        glfwSetMouseButtonCallback(window.window, nullptr);
        glfwSetScrollCallback(window.window, nullptr);
        break;
    case State::AppMode::eEditor:
        gui::destroy();
        break;
    case State::AppMode::eNone:
    default:;
    }

    // init new mode
    switch (mode) {
    case State::AppMode::eViewer:
        glfwSetMouseButtonCallback(window.window, dopbvh_glfw::mouseButtonCallback);
        glfwSetScrollCallback(window.window, dopbvh_glfw::scrollCallback);
        break;
    case State::AppMode::eEditor:
        gui::create(window);
        backend.InitGUIRenderer((directory.res / "font/cascadia/CascadiaMono.ttf").string().c_str());
        break;
    case State::AppMode::eNone:
    default:;
    }

    state.appMode = mode;
}

void Application::saveImage_TMP(std::string path)
{
    if (sceneRenderer.backend.pt_compute == nullptr)
        return;

    auto ctx { backend.Get_TMP() };

    auto const x { sceneRenderer.GetCamera().screenResolution.x };
    auto const y { sceneRenderer.GetCamera().screenResolution.y };
    std::vector<glm::vec4> imgResult;
    imgResult.resize(x * y);

    switch (backend::vulkan::State::Renderer(backend.state.selectedRenderMode_TMP)) {
    case backend::vulkan::State::Renderer::ePathTracingKHR:
        ctx.transfer.FromDevice(sceneRenderer.backend.pt_KHR->GetTarget_TMP(), imgResult);
        break;
    case backend::vulkan::State::Renderer::ePathTracingCompute:
        ctx.transfer.FromDevice(sceneRenderer.backend.pt_compute->GetTarget_TMP(), imgResult);
        break;
    case backend::vulkan::State::Renderer::eDebug:
        ctx.transfer.FromDevice(sceneRenderer.backend.debugView->GetTarget_TMP(), imgResult);
        break;
    }

    std::vector<uint8_t> ldrImg;
    ldrImg.reserve(x * y * 4);
    for (auto& c : imgResult) {
        c *= 255.f;
        ldrImg.emplace_back(std::clamp(static_cast<uint8_t>(c.x), static_cast<uint8_t>(0), static_cast<uint8_t>(255)));
        ldrImg.emplace_back(std::clamp(static_cast<uint8_t>(c.y), static_cast<uint8_t>(0), static_cast<uint8_t>(255)));
        ldrImg.emplace_back(std::clamp(static_cast<uint8_t>(c.z), static_cast<uint8_t>(0), static_cast<uint8_t>(255)));
        ldrImg.emplace_back(static_cast<uint8_t>(255));
    }
    path.append(".png");
    image::write(path.c_str(), x, y, 4, ldrImg.data(), x * 4);
}
