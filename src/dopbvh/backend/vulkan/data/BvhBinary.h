#pragma once

#include <vLime/Memory.h>
#include <vLime/vLime.h>

namespace backend::vulkan::data {

class BvhBinary {
private:
    vk::Device d;
    lime::MemoryManager& memory;

public:
    u32 triCount { 0 };
    u32 nodeCount { 0 };

    lime::Buffer bvh;
    lime::Buffer triangles;
    lime::Buffer triangleIndices;
    lime::Buffer aux;

    BvhBinary(vk::Device d, lime::MemoryManager& memory)
        : d(d)
        , memory(memory)
    {
    }

    template<class Node>
    BvhBinary(vk::Device d, lime::MemoryManager& memory, u32 triCount, u32 nodeCount)
        : d(d)
        , memory(memory)
    {
        Allocate<Node>(triCount, nodeCount);
    }

    template<class Node>
    void Allocate(u32 _triCount, u32 _nodeCount)
    {
        triCount = _triCount;
        nodeCount = _nodeCount;

        bvh.reset();
        triangles.reset();
        triangleIndices.reset();
        aux.reset();

        bvh = memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal },
            {
                .size = nodeCount * Node::SCALAR_SIZE,
                .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            },
            "bvh_binary");

        triangles = memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal },
            {
                .size = triCount * sizeof(f32) * 12,
                .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
            },
            "bvh_binary_triangles");

        triangleIndices = memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal },
            {
                .size = triCount * sizeof(u32) * 2,
                .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
            },
            "bvh_binary_triangle_indices");
    }

    void AllocateAux_dop14Split_TMP()
    {
        aux.reset();
        aux = memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal },
            {
                .size = nodeCount * 16 * sizeof(f32),
                .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress,
            },
            "bvh_aux_dop14_split");
    }
};
}
