#pragma once

#include "../ApplicationState.h"

#include "../backend/Config.h"
#include "../backend/Stats.h"
#include "../backend/vulkan/Vulkan.h"

class Application;
namespace module {

class Benchmark {
public:
    explicit Benchmark(Application& app);
    void ProcessGUI(State& state);

    void LoadConfig();
    void SetupBenchmarkRun();
    void Run();
    void ExportTexTableRows();

    void SetScene(std::string_view name)
    {
        rt.currentScene = -1;
        auto& scenes { Config::GetScenes() };
        for (i32 i { 0 }; i < csize<i32>(scenes); ++i)
            if (scenes[i].name == name)
                rt.currentScene = i;
    }
    void SetDefaultAsActive();

private:
    Application& app;
    backend::vulkan::Vulkan& backend;

    std::vector<backend::config::BVHPipeline> bPipelines;
    std::vector<std::string> scene;
    std::vector<std::string> method;

    struct {
        std::queue<i32> scenes;
        std::vector<i32> pipelines;

        i32 currentScene { 0 };
        i32 currentPipeline { 0 };
        i32 currentView { 0 };
        bool bRunning { false };
    } rt;

    enum class BenchmarkState {
        eIdle,
        eSceneSet,
        eSceneLoadInProgress,
        ePipelineSet,
        ePipelineGetStats,
        eViewSet,
        eViewGetStats,
        eWaitFrames_5,
    } bState { BenchmarkState::eIdle };

    struct BPipeline {
        std::string name;
        backend::stats::BVHPipeline statsBuild;
        std::vector<backend::stats::Trace> statsTrace;

        f32 pMRps { 0.0f };
        f32 sMRps { 0.0f };
    };

    struct SceneBenchmark {
        std::string name;
        std::vector<BPipeline> pipelines;
    };

    std::vector<SceneBenchmark> sceneBenchmarks;
    void ExportPipeline(BPipeline& p, BPipeline const& pRel);
};

}
