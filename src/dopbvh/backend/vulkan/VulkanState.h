#pragma once

#include <utility>
#include <vLime/types.h>

namespace backend::vulkan {

struct State {
    enum class Renderer {
        ePathTracingKHR,
        ePathTracingCompute,
        eDebug,
    };

    enum class BvhBuilder {
        ePLOCpp,
    };

    enum class MortonCode {
        eMorton30,
        eMorton60,
    };

    enum class Centroid {
        eTriangle,
        eAABB,
    };

    f64 oneSecondAcc { 0. };
    bool oncePerSecond { false };
    f64 hundredMsAcc { 0. };
    bool oncePerHundredMs { false };

    bool vSync { true };

    i32 selectedRenderMode_TMP { std::to_underlying(Renderer::ePathTracingKHR) };
    i32 selectedBvhBuilder_TMP { std::to_underlying(BvhBuilder::ePLOCpp) };

    bool debugRenderWindowOnlySelected { false };
    bool debugRenderWindowScene { true };
    bool debugRenderWindowHeatMap { false };
    bool debugRenderWindowWireframe { false };
    bool readBackRayCount { false };

    u32 bvhCollapsing_c_t { 3 };
    u32 bvhCollapsing_c_i { 2 };
};

}
