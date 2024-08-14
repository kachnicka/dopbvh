#pragma once

#include "../data/ImGuiScene.h"
#include "VertexLayout.h"
#include <vLime/RenderGraph.h>
#include <vLime/vLime.h>

namespace backend::vulkan {

class Renderer_imGUI {
    vk::Device d;
    vk::UniqueSampler flatSampler;

    struct Pipeline {
        using VertexLayout = vertex_layout::ImGUI;
        [[nodiscard]] constexpr static std::vector<std::string_view> GetShaderNames()
        {
            return {
                "imgui.vert.spv",
                "imgui.frag.spv",
            };
        }
    };
    lime::PipelineGraphics pipeline;

public:
    explicit Renderer_imGUI(vk::Device d, lime::ShaderCache& sCache)
        : d(d)
        , flatSampler(getSampler())
        , pipeline(setupPipeline(sCache))
    {
    }

    void recordCommands(vk::CommandBuffer commandBuffer, lime::rg::RasterizationInfo const& info, data::Scene_imGUI& scene)
    {
        lime::debug::BeginDebugLabel(commandBuffer, "imgui");

        if (info.buildPipelines)
            pipeline.build(info.renderPassInfo.renderPass, 0);

        commandBuffer.beginRenderPass(&info.renderPassInfo, vk::SubpassContents::eInline);
        scene.finalize_frame(commandBuffer, pipeline.layout, pipeline.get());
        commandBuffer.endRenderPass();
        lime::debug::EndDebugLabel(commandBuffer);
    }

private:
    [[nodiscard]] vk::UniqueSampler getSampler() const
    {
        vk::SamplerCreateInfo cInfo;
        cInfo.magFilter = vk::Filter::eLinear;
        cInfo.minFilter = vk::Filter::eLinear;
        cInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
        cInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
        cInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
        cInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
        cInfo.minLod = -1000;
        cInfo.maxLod = 1000;
        cInfo.maxAnisotropy = 1.0f;

        return lime::check(d.createSamplerUnique(cInfo));
    }

    [[nodiscard]] lime::PipelineGraphics setupPipeline(lime::ShaderCache& sCache)
    {
        std::vector samplers(data::Scene_imGUI::IMGUI_RENDERABLE_TEXTURE_COUNT, flatSampler.get());
        lime::PipelineGraphics pg {
            d,
            sCache,
            Pipeline::GetShaderNames(),
            [&samplers](auto& lb) {
                lb.SetImmutableSamples(0, 0, samplers.data());
                lb.SetDescriptorArraySize(0, 0, data::Scene_imGUI::IMGUI_RENDERABLE_TEXTURE_COUNT);
            },
        };

        auto& pb { pg.pBuilder };
        pb.SetVertexLayout<Pipeline::VertexLayout>();
        pb.SetAlphaBlending(true);
        pb.SetDynamicState(vk::DynamicState::eViewport, true);
        pb.SetDynamicState(vk::DynamicState::eScissor, true);

        return pg;
    }
};

}
