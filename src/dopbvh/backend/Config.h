#pragma once

#include <string>
#include <vLime/types.h>

namespace backend::config {

enum class BV {
    eNone,
    eAABB,
    eDOP14,
    eOBB,
};

enum class SpaceFilling {
    eMorton32,
    // eMorton64,
};

enum class CompressedLayout {
    eBinaryStandard,
    eBinaryDOP14Split,
};

struct PLOC {
    BV bv { BV::eNone };
    SpaceFilling sfc { SpaceFilling::eMorton32 };
    u32 radius { 16 };

    bool operator==(PLOC const& rhs) const
    {
        return bv == rhs.bv && sfc == rhs.sfc && radius == rhs.radius;
    }
};

struct Collapsing {
    BV bv { BV::eNone };
    u32 maxLeafSize { 15 };
    float c_t { 3.f };
    float c_i { 2.f };

    bool operator==(Collapsing const& rhs) const
    {
        return bv == rhs.bv && maxLeafSize == rhs.maxLeafSize && c_t == rhs.c_t && c_i == rhs.c_i;
    }
};

struct Transformation {
    BV bv { BV::eNone };

    bool operator==(Transformation const& rhs) const
    {
        return bv == rhs.bv;
    }
};

struct Compression {
    BV bv { BV::eNone };
    CompressedLayout layout { CompressedLayout::eBinaryStandard };

    bool operator==(Compression const& rhs) const
    {
        return bv == rhs.bv && layout == rhs.layout;
    }
};

struct Tracer {
    BV bv { BV::eNone };
    u32 workgroupCount { 512 };

    i32 traceMode { 0 };
    u32 bvDepth { 1 };
    bool useSeparateKernels { false };

    bool operator==(Tracer const& rhs) const
    {
        return bv == rhs.bv
            && workgroupCount == rhs.workgroupCount
            // && traceBoundingVolumes == rhs.traceBoundingVolumes
            && traceMode == rhs.traceMode
            && bvDepth == rhs.bvDepth
            && useSeparateKernels == rhs.useSeparateKernels;
    }
};

struct Stats {
    BV bv { BV::eNone };
    float c_t { 1.f };
    float c_i { 1.f };

    bool operator==(Stats const& rhs) const
    {
        return bv == rhs.bv && c_t == rhs.c_t && c_i == rhs.c_i;
    }
};

struct BVHPipeline {
    std::string name;

    PLOC plocpp;
    Collapsing collapsing;
    Transformation transformation;
    Compression compression;
    Tracer tracer;

    Stats stats;
};

}
