#pragma once

#include "../RenderUtil.h"
#include <array>

namespace backend::input {

struct Geometry {
    u32 vertexCount { 0 };
    u32 indexCount { 0 };
    void const* vertexData { nullptr };
    void const* indexData { nullptr };
    void const* uvData { nullptr };
    void const* normalData { nullptr };
};

struct Buffer {
    size_t size { 0 };
    void const* data { nullptr };
};

struct Camera {
    std::array<f32, 16> view {};
    std::array<f32, 16> projection {};
    std::array<f32, 16> viewInv {};
    std::array<f32, 16> projectionInv {};
};

struct Texture {
    u32 u { 0 };
    u32 v { 0 };
    Format format { Format::eR8G8B8A8Unorm };
    void* data { nullptr };
};

}
