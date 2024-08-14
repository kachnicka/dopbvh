#pragma once

#include "../../../data/Input.h"
#include <berries/util/UidUtil.h>
#include <vLime/Memory.h>
#include <vLime/Transfer.h>

namespace backend::vulkan::data {

struct Geometry {
    u32 vertexCount { 0 };
    u32 indexCount { 0 };
    lime::Buffer::Detail indexBuffer;
    lime::Buffer::Detail vertexBuffer;
    lime::Buffer::Detail uvBuffer;
    lime::Buffer::Detail normalBuffer;
    void reset()
    {
        vertexBuffer.reset();
        indexBuffer.reset();
        uvBuffer.reset();
        normalBuffer.reset();
    }
};

using ID_Geometry = uid<Geometry>;

class GeometryHandler {
public:
    lime::MemoryManager& m;
    lime::Transfer& t;

private:
    uidVector<Geometry> geometries;
    std::vector<lime::Buffer> buffer;
    std::vector<lime::LinearAllocator> linearAllocator;
    vk::DeviceSize alignment { 0 };

public:
    GeometryHandler(lime::MemoryManager& memory, lime::Transfer& transfer)
        : m(memory)
        , t(transfer)
    {
        allocateBuffer(1);
    }

    Geometry& operator[](ID_Geometry id)
    {
        return geometries[id];
    }

    Geometry const& operator[](ID_Geometry id) const
    {
        return geometries[id];
    }

    [[nodiscard]] auto begin() const
    {
        return geometries.begin();
    }

    [[nodiscard]] auto end() const
    {
        return geometries.end();
    }

    [[nodiscard]] auto size() const
    {
        return geometries.size();
    }

    ID_Geometry add(input::Geometry const& g)
    {
        Geometry geometry {
            .indexBuffer = allocate(sizeof(u32) * g.indexCount),
            .vertexBuffer = allocate(sizeof(f32) * 3 * g.vertexCount),
            .uvBuffer = allocate(sizeof(f32) * 2 * g.vertexCount),
            .normalBuffer = allocate(sizeof(f32) * 3 * g.vertexCount),
        };
        t.ToDeviceSync(g.indexData, geometry.indexBuffer.size, geometry.indexBuffer);
        t.ToDeviceSync(g.vertexData, geometry.vertexBuffer.size, geometry.vertexBuffer);

        if (g.uvData)
            t.ToDeviceSync(g.uvData, geometry.uvBuffer.size, geometry.uvBuffer);
        if (g.normalData)
            t.ToDeviceSync(g.normalData, geometry.normalBuffer.size, geometry.normalBuffer);
        geometry.vertexCount = g.vertexCount;
        geometry.indexCount = g.indexCount;

        return geometries.add(geometry);
    }

    void remove(ID_Geometry id)
    {
        geometries.remove(id);
    }

    void reset()
    {
        geometries.reset();
        buffer.clear();
        linearAllocator.clear();
        allocateBuffer(1);
    }

private:
    lime::Buffer::Detail allocate(vk::DeviceSize size)
    {
        for (auto& la : linearAllocator)
            if (auto b { la.alloc(size, alignment) }; b.resource)
                return b;
        allocateBuffer(size);
        return linearAllocator.back().alloc(size, alignment);
    }

    void allocateBuffer(vk::DeviceSize size)
    {
        using Usage = vk::BufferUsageFlagBits;
        auto usage { Usage::eVertexBuffer | Usage::eIndexBuffer | Usage::eTransferDst };
        if (m.features.bufferDeviceAddress)
            usage |= Usage::eShaderDeviceAddress | Usage::eAccelerationStructureBuildInputReadOnlyKHR;

        size = std::max(size, 128 * lime::MB);
        buffer.emplace_back(m.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal }, { .size = size, .usage = usage }, std::format("geometry_{}", buffer.size()).c_str()));

        linearAllocator.emplace_back(buffer.back());
        alignment = buffer.back().getMemoryRequirements(m.d).alignment;
    }
};

}
