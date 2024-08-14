#include "Benchmark.h"

#include "../Application.h"
#include "../core/Config.h"

namespace module {

Benchmark::Benchmark(Application& app)
    : app(app)
    , backend(app.backend)
{
    LoadConfig();

    if (backend.pt_compute)
        backend.pt_compute->SetPipelineConfiguration(bPipelines[0]);
}

void Benchmark::LoadConfig()
{
    bPipelines = Config::GetBVHPipelines(app.directory.res / "benchmark.toml", scene, method);
}

void Benchmark::SetupBenchmarkRun()
{
    sceneBenchmarks.clear();
    rt = {};

    for (auto const& name : scene) {
        i32 i = 0;
        for (auto const& s : Config::GetScenes()) {
            if (name == s.name) {
                rt.scenes.push(i);
                break;
            }
            i++;
        }
    }
    for (auto const& name : method) {
        i32 i = 0;
        for (auto const& p : bPipelines) {
            if (name == p.name) {
                rt.pipelines.emplace_back(i);
                break;
            }
            i++;
        }
    }

    backend.state.selectedRenderMode_TMP = 1;
    backend.samplesPerPixel = 64;
    backend.ResetAccumulation();

    rt.bRunning = true;
    bState = BenchmarkState::eSceneSet;
    fmt::print("  & BV & SA leaves & rel & SA internal & rel & SA total & rel & avg. leaf size & pMRpS & rel & sMRpS & rel & build time \\\\\n");
}
void Benchmark::Run()
{
    switch (bState) {
    case BenchmarkState::eIdle:
        return;
    case BenchmarkState::eSceneSet: {
        if (rt.scenes.empty()) {
            bState = BenchmarkState::eIdle;
            rt.bRunning = false;
            // ExportTexTableRows();
            return;
        }
        rt.currentScene = rt.scenes.front();
        rt.scenes.pop();

        app.state.sceneToLoad = Config::GetScenes()[rt.currentScene];
        sceneBenchmarks.push_back({ .name = app.state.sceneToLoad.name, .pipelines = {} });
        fmt::print("{} \\\\ \n", app.state.sceneToLoad.name);
        bState = BenchmarkState::eSceneLoadInProgress;
    } break;
    case BenchmarkState::eSceneLoadInProgress: {
        if (backend.selectedScene)
            bState = BenchmarkState::ePipelineSet;
    } break;
    case BenchmarkState::ePipelineSet: {
        static int rtPipelineIdx { 0 };
        if (rtPipelineIdx >= csize<i32>(rt.pipelines)) {
            rtPipelineIdx = 0;
            bState = BenchmarkState::eSceneSet;
            return;
        }
        rt.currentPipeline = rt.pipelines[rtPipelineIdx++];
        auto& pCfg { bPipelines[rt.currentPipeline] };
        backend.pt_compute->SetPipelineConfiguration(pCfg);
        sceneBenchmarks.back().pipelines.push_back({ .name = pCfg.name, .statsBuild = {}, .statsTrace = {} });
        bState = BenchmarkState::ePipelineGetStats;
    } break;
    case BenchmarkState::ePipelineGetStats: {
        sceneBenchmarks.back().pipelines.back().statsBuild = backend.pt_compute->GetStatsBuild();
        bState = BenchmarkState::eViewSet;
    } break;
    case BenchmarkState::eViewSet: {
        if (rt.currentView >= csize<i32>(app.cameraManager.GetCameras())) {
            rt.currentView = 0;
            bState = BenchmarkState::ePipelineSet;
            ExportPipeline(sceneBenchmarks.back().pipelines.back(), sceneBenchmarks.back().pipelines[0]);
            return;
        }
        app.cameraManager.SetActiveCamera(rt.currentView++);
        bState = BenchmarkState::eWaitFrames_5;
    } break;
    case BenchmarkState::eWaitFrames_5: {
        static int frames { 0 };
        if (frames++ >= 5) {
            frames = 0;
            bState = BenchmarkState::eViewGetStats;
        }
    } break;
    case BenchmarkState::eViewGetStats: {
        static int samples { 0 };
        if (samples++ > 7) {
            samples = 0;
            bState = BenchmarkState::eViewSet;
        }
        if (samples > 2)
            sceneBenchmarks.back().pipelines.back().statsTrace.push_back(backend.pt_compute->GetStatsTrace());

    } break;
    }
}

void Benchmark::ExportTexTableRows()
{
    for (auto& s : sceneBenchmarks) {
        fmt::print("{} \\\\ \n", s.name);
        for (auto& p : s.pipelines)
            ExportPipeline(p, s.pipelines[0]);
    }
}
void Benchmark::SetDefaultAsActive()
{
    backend.pt_compute->SetPipelineConfiguration(bPipelines[0]);
    rt.currentPipeline = 0;
}

void Benchmark::ExportPipeline(BPipeline& p, BPipeline const& pRel)
{
    u64 pRayCount { 0 };
    f32 pTraceTimeMs { 0.f };
    u64 sRayCount { 0 };
    f32 sTraceTimeMs { 0.f };
    for (auto const& t : p.statsTrace) {
        pRayCount += t.data[0].rayCount;
        pTraceTimeMs += t.data[0].traceTimeMs;
        for (int i = 1; i < 8; i++) {
            sRayCount += t.data[i].rayCount;
            sTraceTimeMs += t.data[i].traceTimeMs;
        }
    }
    p.pMRps = (static_cast<f32>(pRayCount) * 1e-6f) / (pTraceTimeMs * 1e-3f);
    p.sMRps = (static_cast<f32>(sRayCount) * 1e-6f) / (sTraceTimeMs * 1e-3f);

    std::string_view name { p.name };
    f32 sal { p.statsBuild.compression.saIntersect };
    f32 sal_r { sal / pRel.statsBuild.compression.saIntersect };
    f32 sai { p.statsBuild.compression.saTraverse };
    f32 sai_r { sai / pRel.statsBuild.compression.saTraverse };
    f32 avgl { p.statsBuild.compression.leafSizeAvg };
    f32 sat { p.statsBuild.compression.costTotal };
    f32 sat_r { sat / pRel.statsBuild.compression.costTotal };
    f32 pMRps { p.pMRps };
    f32 pMRps_r { pMRps / pRel.pMRps };
    f32 sMRps { p.sMRps };
    f32 sMRps_r { sMRps / pRel.sMRps };
    f32 buildTime { p.statsBuild.plocpp.timeTotal + p.statsBuild.collapsing.timeTotal + p.statsBuild.transformation.timeTotal + p.statsBuild.compression.timeTotal };
    // empty & BV & SA leaves & rel & SA internal & rel & SA total & rel & avg. leaf size & pMRpS & rel & sMRpS & rel & build time
    fmt::print(" & {} & {:.1f} & ({:.2f}) & {:.1f} & ({:.2f}) & {:.1f} & {:.1f} & ({:.2f}) & {:.1f} & ({:.2f}) & {:.1f} & ({:.2f}) & {:.1f} \\\\\n",
        name, sai, sai_r, sal, sal_r, avgl, sat, sat_r, pMRps, pMRps_r, sMRps, sMRps_r, buildTime);
}

}
