#pragma once

#include "../data/Scene.h"
#include "VertexLayout.h"
#include <vLime/PipelineBuilder.h>
#include <vLime/Reflection.h>
#include <vLime/RenderGraph.h>
#include <vLime/vLime.h>

namespace backend::vulkan {

class AabbRenderer_Stencil {
    vk::Device d;

    struct Pipeline {
        using VertexLayout = vertex_layout::Empty;
        [[nodiscard]] constexpr static std::vector<std::string_view> GetShaderNames()
        {
            return {
                "aabb_stencil.vert.spv",
                "aabb_stencil.frag.spv",
            };
        }
    };
    lime::PipelineGraphics pipeline;

public:
    struct DataDescription {
    private:
        vk::Device d;
        vk::UniqueDescriptorPool dPool;

    public:
        vk::DescriptorSet setFrame;
        vk::DescriptorSet setDraw;

        explicit DataDescription(vk::Device d)
            : d(d)
            , dPool(createDescriptorPool())
        {
        }

        void Describe(lime::PipelineLayout const& pLayout, lime::Buffer::Detail const& camera, lime::Buffer::Detail const& aabb, lime::Buffer::Detail const& transforms)
        {
            // TODO: check here for layout change and possible dSet reallocation
            if (!setFrame || !setDraw) {
                vk::DescriptorSetAllocateInfo allocInfo = {};
                allocInfo.descriptorPool = dPool.get();
                std::array<vk::DescriptorSetLayout, 2> setLayouts { pLayout.dSet[0].get(), pLayout.dSet[1].get() };
                allocInfo.descriptorSetCount = static_cast<u32>(setLayouts.size());
                allocInfo.pSetLayouts = setLayouts.data();

                auto dSets { lime::check(d.allocateDescriptorSets(allocInfo)) };
                setFrame = dSets[0];
                setDraw = dSets[1];
            }

            vk::DescriptorBufferInfo dBuffer[3];
            dBuffer[0].buffer = camera.resource;
            dBuffer[0].offset = camera.offset;
            dBuffer[0].range = camera.size;

            // TODO: propagate size of one uniform buffer
            dBuffer[1].buffer = aabb.resource;
            dBuffer[1].offset = aabb.offset;
            dBuffer[1].range = 64;

            dBuffer[2].buffer = transforms.resource;
            dBuffer[2].offset = transforms.offset;
            dBuffer[2].range = 128;

            vk::WriteDescriptorSet dWrite[3];
            dWrite[0].dstSet = setFrame;
            dWrite[0].dstBinding = 0;
            dWrite[0].descriptorCount = 1;
            dWrite[0].descriptorType = vk::DescriptorType::eUniformBufferDynamic;
            dWrite[0].pBufferInfo = &dBuffer[0];

            dWrite[1].dstSet = setDraw;
            dWrite[1].dstBinding = 0;
            dWrite[1].descriptorCount = 1;
            dWrite[1].descriptorType = vk::DescriptorType::eUniformBufferDynamic;
            dWrite[1].pBufferInfo = &dBuffer[1];

            dWrite[2].dstSet = setDraw;
            dWrite[2].dstBinding = 1;
            dWrite[2].descriptorCount = 1;
            dWrite[2].descriptorType = vk::DescriptorType::eUniformBufferDynamic;
            dWrite[2].pBufferInfo = &dBuffer[2];

            d.updateDescriptorSets(3, dWrite, 0, nullptr);
        }

    private:
        [[nodiscard]] vk::UniqueDescriptorPool createDescriptorPool() const
        {
            std::vector<vk::DescriptorPoolSize> poolSizes {
                { vk::DescriptorType::eUniformBufferDynamic, 3 },
            };
            vk::DescriptorPoolCreateInfo cInfo {
                .maxSets = 2,
                .poolSizeCount = static_cast<u32>(poolSizes.size()),
                .pPoolSizes = poolSizes.data(),
            };
            return lime::check(d.createDescriptorPoolUnique(cInfo));
        }
    } descriptor;

public:
    AabbRenderer_Stencil(vk::Device d, lime::ShaderCache& sCache)
        : d(d)
        , pipeline(setupPipeline(sCache))
        , descriptor(d)
    {
    }

    void recordCommands(vk::CommandBuffer commandBuffer, lime::rg::RasterizationInfo const& info, data::Scene& scene)
    {
        lime::debug::BeginDebugLabel(commandBuffer, "aabb debug: overlap stencil ", lime::debug::LabelColor::eGreen);

        if (info.buildPipelines)
            pipeline.build(info.renderPassInfo.renderPass, 0);

        commandBuffer.beginRenderPass(&info.renderPassInfo, vk::SubpassContents::eInline);

        vk::Viewport viewport { 0.0f, 0.0f,
            static_cast<f32>(info.renderPassInfo.renderArea.extent.width),
            static_cast<f32>(info.renderPassInfo.renderArea.extent.height),
            0.0f, 1.0f };
        vk::Rect2D scissor { info.renderPassInfo.renderArea };

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline.get());
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.getLayout(), 0, descriptor.setFrame, { 0 });
        commandBuffer.setViewport(0, 1, &viewport);
        commandBuffer.setScissor(0, 1, &scissor);

        auto const cnt { static_cast<u32>(scene.localGeometryId.size()) };
        for (u32 i { 0 }; i < cnt; i++) {
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline.getLayout(), 1, descriptor.setDraw, { scene.localGeometryId[i] * 64, i * 128 });
            commandBuffer.draw(3, 12, 0, 0);
        }

        commandBuffer.endRenderPass();
        lime::debug::EndDebugLabel(commandBuffer);
    }

    void Describe(lime::Buffer::Detail const& camera, lime::Buffer::Detail const& aabb, lime::Buffer::Detail const& transforms)
    {
        descriptor.Describe(pipeline.layout, camera, aabb, transforms);
    }

private:
    [[nodiscard]] lime::PipelineGraphics setupPipeline(lime::ShaderCache& sCache)
    {
        lime::PipelineGraphics pg { d, sCache, Pipeline::GetShaderNames() };
        auto& pb { pg.pBuilder };
        pb.SetVertexLayout<Pipeline::VertexLayout>();
        pb.SetDynamicState(vk::DynamicState::eViewport, true);
        pb.SetDynamicState(vk::DynamicState::eScissor, true);
        pb.rasterizer.cullMode = vk::CullModeFlagBits::eFront;
        pb.depthStencil.stencilTestEnable = VK_TRUE;
        pb.depthStencil.front.compareOp = vk::CompareOp::eAlways;
        pb.depthStencil.front.passOp = vk::StencilOp::eIncrementAndClamp;
        pb.depthStencil.front.failOp = vk::StencilOp::eIncrementAndClamp;
        pb.depthStencil.front.depthFailOp = vk::StencilOp::eIncrementAndClamp;
        pb.depthStencil.front.compareMask = 0xff;
        pb.depthStencil.front.writeMask = 0xff;
        pb.depthStencil.front.reference = 1;
        pb.depthStencil.back = pb.depthStencil.front;

        return pg;
    }
};

class AabbRenderer_HeatWireframe {
    vk::Device d;
    vk::UniqueSampler flatSampler;

    struct PipelineScene {
        using VertexLayout = vertex_layout::Scene;
        [[nodiscard]] constexpr static std::vector<std::string_view> GetShaderNames()
        {
            return {
                "scene_simple.vert.spv",
                "scene_simple.frag.spv",
            };
        }
    };

    struct PipelineHeat {
        using VertexLayout = vertex_layout::Empty;
        [[nodiscard]] constexpr static std::vector<std::string_view> GetShaderNames()
        {
            return {
                "fullscreen.vert.spv",
                "aabb_heat_map.frag.spv",
            };
        }
    };

    struct PipelineWireframe {
        using VertexLayout = vertex_layout::Empty;
        [[nodiscard]] constexpr static std::vector<std::string_view> GetShaderNames()
        {
            return {
                "aabb_wireframe.vert.spv",
                "aabb_wireframe.frag.spv",
            };
        }
    };
    lime::PipelineGraphics pipelineScene;
    lime::PipelineGraphics pipelineHeat;
    lime::PipelineGraphics pipelineWireframe;

public:
    u32 selectedNode { lime::INVALID_ID };
    bool renderOnlySelected { false };
    bool renderScene { true };
    bool renderHeatMap { false };
    bool renderWireframe { false };
    f32 heatMapScale { 16.f };

    struct DataDescriptionScene {
    private:
        vk::Device d;
        vk::UniqueDescriptorPool dPool;

    public:
        vk::DescriptorSet setFrame;
        vk::DescriptorSet setDraw;

        explicit DataDescriptionScene(vk::Device d)
            : d(d)
            , dPool(createDescriptorPool())
        {
        }

        void Describe(lime::PipelineLayout const& pLayout, lime::Buffer::Detail const& camera, lime::Buffer::Detail const& transforms)
        {
            // TODO: check here for layout change and possible dSet reallocation
            if (!setFrame || !setDraw) {
                vk::DescriptorSetAllocateInfo allocInfo = {};
                allocInfo.descriptorPool = dPool.get();
                std::array<vk::DescriptorSetLayout, 2> setLayouts { pLayout.dSet[0].get(), pLayout.dSet[1].get() };
                allocInfo.descriptorSetCount = static_cast<u32>(setLayouts.size());
                allocInfo.pSetLayouts = setLayouts.data();

                auto dSets { lime::check(d.allocateDescriptorSets(allocInfo)) };
                setFrame = dSets[0];
                setDraw = dSets[1];
            }

            vk::DescriptorBufferInfo dBuffer[2];
            dBuffer[0].buffer = camera.resource;
            dBuffer[0].offset = camera.offset;
            dBuffer[0].range = camera.size;

            // TODO: propagate size of one uniform buffer
            dBuffer[1].buffer = transforms.resource;
            dBuffer[1].offset = transforms.offset;
            dBuffer[1].range = 128;

            vk::WriteDescriptorSet dWrite[2];
            dWrite[0].dstSet = setFrame;
            dWrite[0].dstBinding = 0;
            dWrite[0].descriptorCount = 1;
            dWrite[0].descriptorType = vk::DescriptorType::eUniformBufferDynamic;
            dWrite[0].pBufferInfo = &dBuffer[0];

            dWrite[1].dstSet = setDraw;
            dWrite[1].dstBinding = 0;
            dWrite[1].descriptorCount = 1;
            dWrite[1].descriptorType = vk::DescriptorType::eUniformBufferDynamic;
            dWrite[1].pBufferInfo = &dBuffer[1];

            d.updateDescriptorSets(2, dWrite, 0, nullptr);
        }

    private:
        [[nodiscard]] vk::UniqueDescriptorPool createDescriptorPool() const
        {
            std::vector<vk::DescriptorPoolSize> poolSizes {
                { vk::DescriptorType::eUniformBufferDynamic, 2 },
            };
            vk::DescriptorPoolCreateInfo cInfo;
            cInfo.maxSets = 2;
            cInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
            cInfo.pPoolSizes = poolSizes.data();
            return lime::check(d.createDescriptorPoolUnique(cInfo));
        }
    } descriptorScene;

    struct DataDescriptionHeatMap {
    private:
        vk::Device d;
        vk::UniqueDescriptorPool dPool;

    public:
        vk::DescriptorSet set;

        explicit DataDescriptionHeatMap(vk::Device d)
            : d(d)
            , dPool(createDescriptorPool())
        {
        }

        void Describe(lime::PipelineLayout const& pLayout, vk::ImageView stencil, vk::Sampler sampler)
        {
            // TODO: check here for layout change and possible dSet reallocation
            if (!set) {
                vk::DescriptorSetAllocateInfo allocInfo = {};
                allocInfo.descriptorPool = dPool.get();
                allocInfo.descriptorSetCount = 1;
                allocInfo.pSetLayouts = &pLayout.dSet[0].get();
                set = lime::check(d.allocateDescriptorSets(allocInfo)).back();
            }

            vk::DescriptorImageInfo dImage[1];
            dImage[0].imageView = stencil;
            dImage[0].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
            dImage[0].sampler = sampler;

            vk::WriteDescriptorSet dWrite[1];

            dWrite[0].dstSet = set;
            dWrite[0].dstBinding = 0;
            dWrite[0].descriptorCount = 1;
            dWrite[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
            dWrite[0].pImageInfo = dImage;

            d.updateDescriptorSets(1, dWrite, 0, nullptr);
        }

    private:
        [[nodiscard]] vk::UniqueDescriptorPool createDescriptorPool() const
        {
            std::vector<vk::DescriptorPoolSize> poolSizes {
                { vk::DescriptorType::eCombinedImageSampler, 1 },
            };
            vk::DescriptorPoolCreateInfo cInfo;
            cInfo.maxSets = 1;
            cInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
            cInfo.pPoolSizes = poolSizes.data();
            return lime::check(d.createDescriptorPoolUnique(cInfo, nullptr));
        }
    } descriptorHeat;

    struct DataDescriptionWireframe {
    private:
        vk::Device d;
        vk::UniqueDescriptorPool dPool;

    public:
        vk::DescriptorSet setFrame;
        vk::DescriptorSet setDraw;

        explicit DataDescriptionWireframe(vk::Device d)
            : d(d)
            , dPool(createDescriptorPool())
        {
        }

        void Describe(lime::PipelineLayout const& pLayout, lime::Buffer::Detail const& camera, lime::Buffer::Detail const& aabb, lime::Buffer::Detail const& transforms)
        {
            // TODO: check here for layout change and possible dSet reallocation
            if (!setFrame || !setDraw) {
                vk::DescriptorSetAllocateInfo allocInfo = {};
                allocInfo.descriptorPool = dPool.get();
                std::array<vk::DescriptorSetLayout, 2> setLayouts { pLayout.dSet[0].get(), pLayout.dSet[1].get() };
                allocInfo.descriptorSetCount = static_cast<u32>(setLayouts.size());
                allocInfo.pSetLayouts = setLayouts.data();

                auto dSets { lime::check(d.allocateDescriptorSets(allocInfo)) };
                setFrame = dSets[0];
                setDraw = dSets[1];
            }

            vk::DescriptorBufferInfo dBuffer[3];
            dBuffer[0].buffer = camera.resource;
            dBuffer[0].offset = camera.offset;
            dBuffer[0].range = camera.size;

            // TODO: propagate size of one uniform buffer
            dBuffer[1].buffer = aabb.resource;
            dBuffer[1].offset = aabb.offset;
            dBuffer[1].range = 64;

            dBuffer[2].buffer = transforms.resource;
            dBuffer[2].offset = transforms.offset;
            dBuffer[2].range = 128;

            vk::WriteDescriptorSet dWrite[3];
            dWrite[0].dstSet = setFrame;
            dWrite[0].dstBinding = 0;
            dWrite[0].descriptorCount = 1;
            dWrite[0].descriptorType = vk::DescriptorType::eUniformBufferDynamic;
            dWrite[0].pBufferInfo = &dBuffer[0];

            dWrite[1].dstSet = setDraw;
            dWrite[1].dstBinding = 0;
            dWrite[1].descriptorCount = 1;
            dWrite[1].descriptorType = vk::DescriptorType::eUniformBufferDynamic;
            dWrite[1].pBufferInfo = &dBuffer[1];

            dWrite[2].dstSet = setDraw;
            dWrite[2].dstBinding = 1;
            dWrite[2].descriptorCount = 1;
            dWrite[2].descriptorType = vk::DescriptorType::eUniformBufferDynamic;
            dWrite[2].pBufferInfo = &dBuffer[2];

            d.updateDescriptorSets(3, dWrite, 0, nullptr);
        }

    private:
        [[nodiscard]] vk::UniqueDescriptorPool createDescriptorPool() const
        {
            std::vector<vk::DescriptorPoolSize> poolSizes {
                { vk::DescriptorType::eUniformBufferDynamic, 3 },
            };
            vk::DescriptorPoolCreateInfo cInfo;
            cInfo.maxSets = 2;
            cInfo.poolSizeCount = static_cast<u32>(poolSizes.size());
            cInfo.pPoolSizes = poolSizes.data();
            return lime::check(d.createDescriptorPoolUnique(cInfo));
        }
    } descriptorWireframe;

public:
    AabbRenderer_HeatWireframe(vk::Device d, lime::ShaderCache& sCache)
        : d(d)
        , flatSampler(getSampler())
        , pipelineScene(setupPipelineScene(sCache))
        , pipelineHeat(setupPipelineHeat(sCache))
        , pipelineWireframe(setupPipelineWireframe(sCache))
        , descriptorScene(d)
        , descriptorHeat(d)
        , descriptorWireframe(d)
    {
    }

    void recordCommands(vk::CommandBuffer commandBuffer, lime::rg::RasterizationInfo const& info, data::Scene& scene)
    {
        lime::debug::BeginDebugLabel(commandBuffer, "aabb debug", lime::debug::LabelColor::eGreen);

        if (info.buildPipelines) {
            pipelineScene.build(info.renderPassInfo.renderPass, 0);
            pipelineHeat.build(info.renderPassInfo.renderPass, 0);
            pipelineWireframe.build(info.renderPassInfo.renderPass, 0);
        }

        commandBuffer.beginRenderPass(&info.renderPassInfo, vk::SubpassContents::eInline);

        vk::Viewport viewport { 0.0f, 0.0f,
            static_cast<f32>(info.renderPassInfo.renderArea.extent.width),
            static_cast<f32>(info.renderPassInfo.renderArea.extent.height),
            0.0f, 1.0f };
        vk::Rect2D scissor { info.renderPassInfo.renderArea };

        if (renderScene) {
            lime::debug::BeginDebugLabel(commandBuffer, "aabb debug: scene ", lime::debug::LabelColor::eGreen);
            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineScene.get());
            commandBuffer.setViewport(0, 1, &viewport);
            commandBuffer.setScissor(0, 1, &scissor);
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineScene.getLayout(), 0, descriptorScene.setFrame, { 0 });

            auto const renderSelection { renderOnlySelected && selectedNode != lime::INVALID_ID };
            auto const& idsToRender { renderSelection ? scene.sceneNodeToRenderedNodes[selectedNode] : scene.localGeometryId };
            for (auto const& nodeIdLocal : idsToRender) {
                auto const& g { scene.data->geometries[scene.nodeGeometry[nodeIdLocal]] };
                u32 const geometryBasedColorSeed { renderHeatMap ? lime::INVALID_ID : scene.localGeometryId[nodeIdLocal] };
                commandBuffer.pushConstants(pipelineScene.getLayout(), vk::ShaderStageFlagBits::eVertex, sizeof(u32) * 0, sizeof(u32) * 1, &geometryBasedColorSeed);
                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineScene.getLayout(), 1, descriptorScene.setDraw, { nodeIdLocal * 128 });
                commandBuffer.bindVertexBuffers(0, { g.vertexBuffer.resource, g.normalBuffer.resource }, { g.vertexBuffer.offset, g.normalBuffer.offset });
                commandBuffer.bindIndexBuffer(g.indexBuffer.resource, g.indexBuffer.offset, vk::IndexType::eUint32);
                commandBuffer.drawIndexed(g.indexCount, 1, 0, 0, 0);
            }
            lime::debug::EndDebugLabel(commandBuffer);
        }

        if (renderHeatMap) {
            struct {
                f32 alpha { 1.f };
                f32 scale { 1.0 };
            } pc;
            pc.alpha = renderScene ? .75f : 1.f;
            pc.scale = heatMapScale;

            lime::debug::BeginDebugLabel(commandBuffer, "aabb debug: overlap heat map ", lime::debug::LabelColor::eGreen);
            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineHeat.get());
            commandBuffer.pushConstants(pipelineHeat.getLayout(), vk::ShaderStageFlagBits::eFragment, sizeof(f32) * 0, sizeof(f32) * 2, &pc);
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineHeat.getLayout(), 0, descriptorHeat.set, nullptr);
            commandBuffer.setViewport(0, 1, &viewport);
            commandBuffer.setScissor(0, 1, &scissor);
            commandBuffer.draw(3, 1, 0, 0);
            lime::debug::EndDebugLabel(commandBuffer);
        }

        if (renderWireframe) {
            lime::debug::BeginDebugLabel(commandBuffer, "aabb debug: overlap wireframe ", lime::debug::LabelColor::eGreen);
            commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineWireframe.get());
            commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineWireframe.getLayout(), 0, descriptorWireframe.setFrame, { 0 });
            commandBuffer.setViewport(0, 1, &viewport);

            auto const renderSelection { renderOnlySelected && selectedNode != lime::INVALID_ID };
            auto const& idsToRender { renderSelection ? scene.sceneNodeToRenderedNodes[selectedNode] : scene.localGeometryId };
            for (auto const& nodeIdLocal : idsToRender) {
                commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineWireframe.getLayout(), 1, descriptorWireframe.setDraw, { scene.localGeometryId[nodeIdLocal] * 64, nodeIdLocal * 128 });
                commandBuffer.draw(3, 12, 0, 0);
            }
            lime::debug::EndDebugLabel(commandBuffer);
        }

        commandBuffer.endRenderPass();
        lime::debug::EndDebugLabel(commandBuffer);
    }

    void Describe(lime::Buffer::Detail const& camera, vk::ImageView stencil, lime::Buffer::Detail const& aabb, lime::Buffer::Detail const& transforms)
    {
        descriptorScene.Describe(pipelineScene.layout, camera, transforms);
        descriptorHeat.Describe(pipelineHeat.layout, stencil, flatSampler.get());
        descriptorWireframe.Describe(pipelineWireframe.layout, camera, aabb, transforms);
    }

private:
    [[nodiscard]] vk::UniqueSampler getSampler() const
    {
        vk::SamplerCreateInfo cInfo;

        cInfo.magFilter = vk::Filter::eNearest;
        cInfo.minFilter = vk::Filter::eNearest;
        cInfo.mipmapMode = vk::SamplerMipmapMode::eNearest;
        cInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
        cInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
        cInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
        cInfo.minLod = -1000;
        cInfo.maxLod = 1000;
        cInfo.maxAnisotropy = 1.0f;

        return lime::check(d.createSamplerUnique(cInfo));
    }

    [[nodiscard]] lime::PipelineGraphics setupPipelineScene(lime::ShaderCache& sCache)
    {
        lime::PipelineGraphics pg { d, sCache, PipelineScene::GetShaderNames() };

        auto& pb { pg.pBuilder };
        pb.SetVertexLayout<PipelineScene::VertexLayout>();
        pb.SetDynamicState(vk::DynamicState::eViewport, true);
        pb.SetDynamicState(vk::DynamicState::eScissor, true);
        pb.SetMultiSampling(vk::SampleCountFlagBits::e4);
        pb.SetDepthTest(true);

        return pg;
    }

    [[nodiscard]] lime::PipelineGraphics setupPipelineHeat(lime::ShaderCache& sCache)
    {
        lime::PipelineGraphics pg { d, sCache, PipelineHeat::GetShaderNames() };

        auto& pb { pg.pBuilder };
        pb.SetVertexLayout<PipelineHeat::VertexLayout>();
        pb.SetDynamicState(vk::DynamicState::eViewport, true);
        pb.SetDynamicState(vk::DynamicState::eScissor, true);
        pb.SetMultiSampling(vk::SampleCountFlagBits::e4);
        pb.SetAlphaBlending(true);

        return pg;
    }

    [[nodiscard]] lime::PipelineGraphics setupPipelineWireframe(lime::ShaderCache& sCache)
    {
        lime::PipelineGraphics pg { d, sCache, PipelineWireframe::GetShaderNames() };

        auto& pb { pg.pBuilder };
        pb.SetVertexLayout<PipelineWireframe::VertexLayout>();
        pb.SetWireframe(true);
        pb.SetDynamicState(vk::DynamicState::eViewport, true);
        pb.SetDynamicState(vk::DynamicState::eScissor, true);
        pb.SetMultiSampling(vk::SampleCountFlagBits::e4);

        return pg;
    }
};

}
