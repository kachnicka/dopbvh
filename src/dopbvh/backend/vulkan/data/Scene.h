#pragma once

#include "../../../scene/Scene.h"
#include "DeviceData.h"
#include "handler/Geometry.h"
#include <berries/util/UidUtil.h>
#include <vector>

namespace backend::vulkan::data {

struct Scene {
    DeviceData* data { nullptr };

    ::Scene::AABB aabb;
    u32 totalTriangleCount { 0 };
    std::vector<ID_Geometry> geometries;

    std::vector<std::array<f32, 16>> toWorld;
    std::vector<ID_Geometry> nodeGeometry;
    std::vector<u32> localGeometryId;

    std::vector<std::vector<u32>> sceneNodeToRenderedNodes;

    lime::Buffer aabbBuffer;
    lime::Buffer transformBuffer;

    using bfub = vk::BufferUsageFlagBits;
    void addAABBs_TMP(input::Buffer const& buffer)
    {
        aabbBuffer.reset();
        aabbBuffer = data->geometries.m.alloc(
            { .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal },
            {
                .size = buffer.size,
                .usage = bfub::eUniformBuffer | bfub::eStorageBuffer | bfub::eTransferDst | bfub::eShaderDeviceAddress | bfub::eAccelerationStructureBuildInputReadOnlyKHR,
            },
            "buf_aabb");
        data->geometries.t.ToDeviceSync(buffer.data, buffer.size, aabbBuffer);
    }

    void addTransforms_TMP(input::Buffer const& buffer)
    {
        transformBuffer.reset();
        transformBuffer = data->geometries.m.alloc(
            { .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal },
            {
                .size = buffer.size,
                .usage = bfub::eUniformBuffer | bfub::eTransferDst,
            },
            "buf_transform");
        data->geometries.t.ToDeviceSync(buffer.data, buffer.size, transformBuffer);
    }
};

}
