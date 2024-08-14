#pragma once

#include "../../RenderUtil.h"
#include "../VCtx.h"
#include "handler/Texture.h"
#include <berries/lib_helper/spdlog.h>
#include <imgui.h>
#include <vLime/Reflection.h>
#include <vLime/vLime.h>

namespace backend::vulkan::data {

struct Scene_imGUI {
    inline static constexpr u32 IMGUI_RENDERABLE_TEXTURE_COUNT { 16 };

    ID_Texture fontId;
    vk::ImageView textureDefault;
    vk::ImageView textureFont;
    vk::ImageView textureRenderedScene;
    vk::ImageView textureDopVisualizer;

    void SetDefaultTexture(vk::ImageView imageView)
    {
        textureDefault = textureFont = textureRenderedScene = textureDopVisualizer = imageView;
    }

private:
    VCtx ctx;

    lime::Buffer buffer;
    lime::LinearAllocator linearAllocator;

    vk::IndexType indexType;

    struct FrameMetadata {
        f32 fBuffer[2] = { 0.0f, 0.0f };
        f32 scale[2] = { 0.0f, 0.0f };
        f32 translate[2] = { 0.0f, 0.0f };
    } metadata;

    struct DataDescription {
    public:
        u32 textureCount { 16 };

    private:
        vk::Device d;
        vk::UniqueDescriptorPool dPool;

    public:
        vk::DescriptorSet set;

        explicit DataDescription(vk::Device d)
            : d(d)
            , dPool(createDescriptorPool())
        {
        }

        void Describe(lime::PipelineLayout const& pLayout, Scene_imGUI const& scene)
        {
            // TODO: check here for layout change and possible dSet reallocation
            if (!set) {
                vk::DescriptorSetAllocateInfo allocInfo = {};
                allocInfo.descriptorPool = dPool.get();
                allocInfo.descriptorSetCount = 1;
                allocInfo.pSetLayouts = &pLayout.dSet[0].get();
                set = lime::check(d.allocateDescriptorSets(allocInfo)).back();
            }

            std::vector<vk::DescriptorImageInfo> dImgInfos;
            dImgInfos.reserve(textureCount);
            vk::ImageView imgView;
            for (unsigned i = 0; i < textureCount; i++) {
                switch (i) {
                case gui::TextureID::font:
                    imgView = scene.textureFont;
                    break;
                case gui::TextureID::renderedScene:
                    imgView = scene.textureRenderedScene;
                    break;
                case gui::TextureID::dopVisualizer:
                    imgView = scene.textureDopVisualizer;
                    break;
                default:
                    imgView = scene.textureDefault;
                    break;
                }
                dImgInfos.emplace_back(vk::Sampler {}, imgView, vk::ImageLayout::eShaderReadOnlyOptimal);
            }

            vk::WriteDescriptorSet dWrite {
                .dstSet = set,
                .descriptorCount = csize<u32>(dImgInfos),
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = dImgInfos.data(),
            };
            d.updateDescriptorSets(1, &dWrite, 0, nullptr);
        }

    private:
        [[nodiscard]] vk::UniqueDescriptorPool createDescriptorPool() const
        {
            std::vector<vk::DescriptorPoolSize> poolSizes {
                { vk::DescriptorType::eCombinedImageSampler, textureCount },
            };
            vk::DescriptorPoolCreateInfo cInfo {
                .maxSets = 1,
                .poolSizeCount = csize<u32>(poolSizes),
                .pPoolSizes = poolSizes.data(),
            };
            return lime::check(d.createDescriptorPoolUnique(cInfo));
        }
    } descriptor;

public:
    explicit Scene_imGUI(VCtx ctx)
        : ctx(ctx)
        , indexType(sizeof(ImDrawIdx) == 2 ? vk::IndexType::eUint16 : vk::IndexType::eUint32)
        , descriptor(ctx.d)
    {
    }

    void finalize_frame(vk::CommandBuffer commandBuffer, lime::PipelineLayout const& pLayout, vk::Pipeline pipeline)
    {
        ImGui::Render();
        auto const* imGuiData { ImGui::GetDrawData() };
        // bool const isMinimized = (imGuiData->DisplaySize.x <= 0.0f || imGuiData->DisplaySize.y <= 0.0f);
        // // Update and Render additional Platform Windows
        // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        //     ImGui::UpdatePlatformWindows();
        //     ImGui::RenderPlatformWindowsDefault();
        // }

        prepareMemoryForFrameData(imGuiData);

        metadata.fBuffer[0] = imGuiData->DisplaySize.x * imGuiData->FramebufferScale.x;
        metadata.fBuffer[1] = imGuiData->DisplaySize.y * imGuiData->FramebufferScale.y;
        metadata.scale[0] = 2.0f / imGuiData->DisplaySize.x;
        metadata.scale[1] = 2.0f / imGuiData->DisplaySize.y;
        metadata.translate[0] = -1.0f - imGuiData->DisplayPos.x * metadata.scale[0];
        metadata.translate[1] = -1.0f - imGuiData->DisplayPos.y * metadata.scale[1];

        ImVec2 const clip_off { imGuiData->DisplayPos };
        ImVec2 const clip_scale { imGuiData->FramebufferScale };
        vk::Rect2D scissors;

        if (metadata.fBuffer[0] <= 0.0f || metadata.fBuffer[1] <= 0.0f)
            return;
        vk::Viewport viewport { 0.0f, 0.0f, metadata.fBuffer[0], metadata.fBuffer[1], 0.0f, 1.0f };

        // TODO: describe only on data change
        descriptor.Describe(pLayout, *this);

        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pLayout.pipeline.get(), 0, { descriptor.set }, nullptr);

        commandBuffer.setViewport(0, 1, &viewport);
        commandBuffer.pushConstants(pLayout.pipeline.get(), vk::ShaderStageFlagBits::eVertex, sizeof(f32) * 0, sizeof(f32) * 2, metadata.scale);
        commandBuffer.pushConstants(pLayout.pipeline.get(), vk::ShaderStageFlagBits::eVertex, sizeof(f32) * 2, sizeof(f32) * 2, metadata.translate);

        for (int n = 0; n < imGuiData->CmdListsCount; n++) {
            ImDrawList const* cmd_list = imGuiData->CmdLists[n];

            auto const vertexBuffer { allocLocalBuffer(cmd_list->VtxBuffer.Size * sizeof(ImDrawVert), cmd_list->VtxBuffer.Data) };
            auto const indexBuffer { allocLocalBuffer(cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx), cmd_list->IdxBuffer.Data) };

            for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++) {
                ImDrawCmd const* pcmd = &cmd_list->CmdBuffer[cmd_i];

                if (pcmd->UserCallback != nullptr) {
                    if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                        ; // ImGui_ImplVulkan_SetupRenderState(draw_data, pipeline, command_buffer, rb, fb_width, fb_height);
                    else
                        pcmd->UserCallback(cmd_list, pcmd);
                } else {
                    ImVec4 clip_rect;
                    clip_rect.x = (pcmd->ClipRect.x - clip_off.x) * clip_scale.x;
                    clip_rect.y = (pcmd->ClipRect.y - clip_off.y) * clip_scale.y;
                    clip_rect.z = (pcmd->ClipRect.z - clip_off.x) * clip_scale.x;
                    clip_rect.w = (pcmd->ClipRect.w - clip_off.y) * clip_scale.y;

                    if (clip_rect.x < metadata.fBuffer[0] && clip_rect.y < metadata.fBuffer[1] && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
                        if (clip_rect.x < 0.0f)
                            clip_rect.x = 0.0f;
                        if (clip_rect.y < 0.0f)
                            clip_rect.y = 0.0f;
                    }

                    scissors.offset.x = static_cast<i32>(clip_rect.x);
                    scissors.offset.y = static_cast<i32>(clip_rect.y);
                    scissors.extent.width = static_cast<u32>(clip_rect.z - clip_rect.x);
                    scissors.extent.height = static_cast<u32>(clip_rect.w - clip_rect.y);

                    auto const texId { static_cast<u32>(reinterpret_cast<intptr_t>(pcmd->TextureId)) };

                    commandBuffer.bindVertexBuffers(0, { vertexBuffer.resource }, { vertexBuffer.offset });
                    commandBuffer.bindIndexBuffer(indexBuffer.resource, indexBuffer.offset, indexType);
                    commandBuffer.setScissor(0, 1, &scissors);
                    commandBuffer.pushConstants(pLayout.pipeline.get(), vk::ShaderStageFlagBits::eFragment, sizeof(f32) * 4, sizeof(u32) * 1, &texId);
                    commandBuffer.drawIndexed(pcmd->ElemCount, 1, pcmd->IdxOffset, pcmd->VtxOffset, 0);
                }
            }
        }
    }

    inline void upload_font_texture(TextureHandler& textureHandler, char const* pathFont)
    {
#ifdef IMGUI_ENABLE_FREETYPE
        berry::Log::debug("FREETYPE font rasterizer");
#endif

        ImGuiIO& io = ImGui::GetIO();
        io.Fonts->AddFontFromFileTTF(pathFont, 14);

        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        fontId = textureHandler.add(static_cast<u32>(width), static_cast<u32>(height), pixels, vk::Format::eR8G8B8A8Unorm);
        textureFont = textureHandler[fontId].imgView.get();
        io.Fonts->SetTexID(reinterpret_cast<ImTextureID>(gui::TextureID::font));
    }

private:
    inline static constexpr size_t memoryReserveFactor { 2 };
    vk::DeviceSize alignment { 0 };

    void prepareMemoryForFrameData(ImDrawData const* imGuiData)
    {
        auto size { imGuiData->TotalVtxCount * sizeof(ImDrawVert) + imGuiData->TotalIdxCount * sizeof(ImDrawIdx) };
        size = lime::memory::align(size * memoryReserveFactor, 1024);

        if (size <= buffer.getSizeInBytes()) {
            linearAllocator.reset();
            return;
        }

        buffer.reset();
        buffer = ctx.memory.alloc(
            {
                .memoryUsage = lime::DeviceMemoryUsage::eHostToDeviceOptimal,
            },
            {
                .size = size,
                .usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer,
            },
            "geometry_imgui");

        linearAllocator = lime::LinearAllocator(buffer);
        alignment = buffer.getMemoryRequirements(ctx.d).alignment;
    }

    lime::Buffer::Detail allocLocalBuffer(vk::DeviceSize size, void const* data)
    {
        auto result { linearAllocator.alloc(size, alignment) };

        if (result.size == 0)
            return {};

        ctx.transfer.ToDeviceSync(data, size, result);
        return result;
    }
};

}
