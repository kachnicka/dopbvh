#pragma once

#include <berries/lib_helper/spdlog.h>
#include <vLime/types.h>

#include <vector>

namespace backend::stats {

struct PLOC {
    std::vector<f32> times;
    f32 timeTotal { 0.f };

    u32 iterationCount { 0 };
    f32 saIntersect { 0.f };
    f32 saTraverse { 0.f };
    f32 costTotal { 0.f };

    u32 nodeCountTotal { 0 };
    u32 leafSizeMin { 0 };
    u32 leafSizeMax { 0 };
    f32 leafSizeAvg { 0.f };

    void print() const
    {
        berry::Log::info("  PLOC:");
        berry::Log::info("    Time total: {:.2f} ms", timeTotal);
        berry::Log::info("{:>14.2f} ms  - init. clusters, woopify", times[0]);
        berry::Log::info("{:>14.2f} ms  - radix sort", times[1]);
        berry::Log::info("{:>14.2f} ms  - copy clusters", times[2]);
        berry::Log::info("{:>14.2f} ms  - PLOC iterations", times[3]);
        berry::Log::info("    Iteration count: {}", iterationCount);
        berry::Log::info("    Cost total: {:.2f}", costTotal);
        berry::Log::info("{:>17.2f}  - area intersect", saIntersect);
        berry::Log::info("{:>17.2f}  - area traverse", saTraverse);
        berry::Log::info("    #Nodes total: {}", nodeCountTotal);
        berry::Log::info("    Leaf size min: {}", leafSizeMin);
        berry::Log::info("    Leaf size max: {}", leafSizeMax);
        berry::Log::info("    Leaf size avg: {:.2f}", leafSizeAvg);
    }
};

struct Transformation {
    f32 timeTotal { 0.f };

    f32 saIntersect { 0.f };
    f32 saTraverse { 0.f };
    f32 costTotal { 0.f };

    u32 nodeCountTotal { 0 };
    u32 leafSizeMin { 0 };
    u32 leafSizeMax { 0 };
    f32 leafSizeAvg { 0.f };

    void print() const
    {
        berry::Log::info("  Transformation:");
        berry::Log::info("    Time total: {:.2f} ms", timeTotal);
        berry::Log::info("    Cost total: {:.2f}", costTotal);
        berry::Log::info("{:>17.2f}  - area intersect", saIntersect);
        berry::Log::info("{:>17.2f}  - area traverse", saTraverse);
        berry::Log::info("    #Nodes total: {}", nodeCountTotal);
        berry::Log::info("    Leaf size min: {}", leafSizeMin);
        berry::Log::info("    Leaf size max: {}", leafSizeMax);
        berry::Log::info("    Leaf size avg: {:.2f}", leafSizeAvg);
    }
};

struct Collapsing {
    f32 timeTotal { 0.f };

    f32 saIntersect { 0.f };
    f32 saTraverse { 0.f };
    f32 costTotal { 0.f };

    u32 nodeCountTotal { 0 };
    u32 leafSizeMin { 0 };
    u32 leafSizeMax { 0 };
    f32 leafSizeAvg { 0.f };

    void print() const
    {
        berry::Log::info("  Collapsing:");
        berry::Log::info("    Time total: {:.2f} ms", timeTotal);
        berry::Log::info("    Cost total: {:.2f}", costTotal);
        berry::Log::info("{:>17.2f}  - area intersect", saIntersect);
        berry::Log::info("{:>17.2f}  - area traverse", saTraverse);
        berry::Log::info("    #Nodes total: {}", nodeCountTotal);
        berry::Log::info("    Leaf size min: {}", leafSizeMin);
        berry::Log::info("    Leaf size max: {}", leafSizeMax);
        berry::Log::info("    Leaf size avg: {:.2f}", leafSizeAvg);
    }
};

struct Compression {
    f32 timeTotal { 0.f };

    f32 saIntersect { 0.f };
    f32 saTraverse { 0.f };
    f32 costTotal { 0.f };

    u32 nodeCountTotal { 0 };
    u32 leafSizeMin { 0 };
    u32 leafSizeMax { 0 };
    f32 leafSizeAvg { 0.f };

    void print() const
    {
        berry::Log::info("  Compression:");
        berry::Log::info("    Time total: {:.2f} ms", timeTotal);
        berry::Log::info("    Cost total: {:.2f}", costTotal);
        berry::Log::info("{:>17.2f}  - area intersect", saIntersect);
        berry::Log::info("{:>17.2f}  - area traverse", saTraverse);
        berry::Log::info("    #Nodes total: {}", nodeCountTotal);
        berry::Log::info("    Leaf size min: {}", leafSizeMin);
        berry::Log::info("    Leaf size max: {}", leafSizeMax);
        berry::Log::info("    Leaf size avg: {:.2f}", leafSizeAvg);
    }
};

struct BVHPipeline {
    PLOC plocpp;
    Collapsing collapsing;
    Transformation transformation;
    Compression compression;

    void print() const
    {
        plocpp.print();
        collapsing.print();
        transformation.print();
        compression.print();
    }
};

struct Trace {
    struct PerDepth {
        u32 rayCount { 0 };
        f32 traceTimeMs { 0.f };
    };
    std::array<PerDepth, 8> data;
};

}
