#pragma once

#include "../VCtx.h"
#include "data_scene.h"
#include "handler/Geometry.h"
#include "handler/Texture.h"
#include <vLime/Memory.h>

namespace backend::vulkan::data {

struct DeviceData {
    VCtx ctx;
    GeometryHandler geometries;
    TextureHandler textures;

    lime::Buffer cameraBuffer;
    lime::Buffer sceneDescriptionBuffer;

    explicit DeviceData(VCtx ctx)
        : ctx(ctx)
        , geometries(ctx.memory, ctx.transfer)
        , textures(ctx.memory, ctx.transfer)
    {
        cameraBuffer = ctx.memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eHostToDeviceOptimal },
            {
                .size = sizeof(input::Camera),
                .usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst,
            },
            "camera");
    }

    void SetCamera_TMP(input::Camera const& c)
    {
        ctx.transfer.ToDeviceSync(&c, sizeof(c), cameraBuffer);
    }

    void SetSceneDescription_TMP()
    {
        std::vector<data_scene::Geometry> data;
        for (auto const& g : geometries) {
            data.push_back({
                .vtxAddress = g.vertexBuffer.getDeviceAddress(ctx.d),
                .idxAddress = g.indexBuffer.getDeviceAddress(ctx.d),
                .normalAddress = g.normalBuffer.getDeviceAddress(ctx.d),
                .uvAddress = g.uvBuffer.getDeviceAddress(ctx.d),
            });
        }
        if (auto const size { data.size() * data_scene::Geometry::SCALAR_SIZE }; sceneDescriptionBuffer.getSizeInBytes() < size) {
            sceneDescriptionBuffer.reset();
            sceneDescriptionBuffer = ctx.memory.alloc({ .memoryUsage = lime::DeviceMemoryUsage::eDeviceOptimal },
                {
                    .size = size,
                    .usage = vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eShaderDeviceAddress | vk::BufferUsageFlagBits::eTransferDst,
                },
                "scene_description");
        }
        ctx.transfer.ToDeviceSync(data, sceneDescriptionBuffer);
    }

    void reset()
    {
        // TODO: reset only geometries now
        geometries.reset();
        sceneDescriptionBuffer.reset();
    }
};

}
