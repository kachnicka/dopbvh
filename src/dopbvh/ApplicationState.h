#pragma once

#include "core/Config.h"

struct State
{
    inline static constexpr bool FAST_EXIT { true };
    enum class AppMode : u8
    {
        eNone,
        eEditor,
        eViewer,
    } appMode = AppMode::eNone;

    struct Windows
    {
        bool demo = false;
        bool logger = true;
        bool renderer = true;
    } modules;

    Config::Scene sceneToLoad {};
    bool sceneLoading = false;
    f32 statusBarFade { 1.0f };
    f32 progress { 0.f };

    bool sceneGraph_maxDepthFound { false };
    u32 sceneGraph_splitDepth { 0 };
    u32 sceneGraph_selectedNode { std::numeric_limits<u32>::max() };

    u64 frameId = 0;
    f64 time { 0. };
    f64 deltaTime { 0. };
};
