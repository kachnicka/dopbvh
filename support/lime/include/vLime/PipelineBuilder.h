#pragma once

#include <vLime/vLime.h>
#include <unordered_set>

namespace lime::pipeline {
class BuilderGraphics {
    vk::Device d;

public:
    std::vector<vk::VertexInputBindingDescription> vertexBindings {};
    std::vector<vk::VertexInputAttributeDescription> vertexAttributes {};
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages {};
    std::vector<vk::PipelineColorBlendAttachmentState> colorBlendAttachments {};
    std::vector<vk::DynamicState> dynamicStates {};
    std::unordered_set<vk::DynamicState> dStateEnabled {};

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo {};
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly {};
    vk::PipelineViewportStateCreateInfo viewportState {};
    vk::PipelineRasterizationStateCreateInfo rasterizer {};
    vk::PipelineMultisampleStateCreateInfo multiSampling {};
    vk::PipelineColorBlendStateCreateInfo colorBlending {};
    vk::PipelineDepthStencilStateCreateInfo depthStencil {};
    vk::PipelineDynamicStateCreateInfo dynamicStateInfo {};
    vk::GraphicsPipelineCreateInfo pipelineInfo {};

    explicit BuilderGraphics(vk::Device d)
        : d(d)
    {
        inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
        rasterizer.lineWidth = 1.0f;

        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;

        colorBlendAttachments.emplace_back();
        colorBlendAttachments.back().colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    }

    void AppendColorBlendAttachment()
    {
        colorBlendAttachments.emplace_back();
        colorBlendAttachments.back().colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    }

    void SetPrimitiveTopology(vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList, vk::Bool32 primitiveRestartEnable = VK_FALSE)
    {
        inputAssembly.topology = topology;
        inputAssembly.primitiveRestartEnable = primitiveRestartEnable;
    }

    void SetWireframe(bool enable)
    {
        if (enable)
            rasterizer.polygonMode = vk::PolygonMode::eLine;
        else
            rasterizer.polygonMode = vk::PolygonMode::eFill;
    }

    void SetDepthBias(bool enable, float factor = 0.f)
    {
        if (enable) {
            rasterizer.depthBiasEnable = VK_TRUE;
            rasterizer.depthBiasConstantFactor = factor;
        } else {
            rasterizer.depthBiasEnable = VK_FALSE;
            rasterizer.depthBiasConstantFactor = 0.0f;
        }
    }

    void SetDepthTest(bool enable)
    {
        if (enable) {
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthWriteEnable = VK_TRUE;
            depthStencil.depthCompareOp = vk::CompareOp::eLess;
        } else {
            depthStencil.depthTestEnable = VK_FALSE;
            depthStencil.depthWriteEnable = VK_FALSE;
            depthStencil.depthCompareOp = vk::CompareOp::eNever;
        }
    }

    void SetStencilSum(bool enable)
    {
        if (enable) {
            depthStencil.stencilTestEnable = VK_TRUE;
            depthStencil.front.passOp = vk::StencilOp::eIncrementAndClamp;
        } else {
            depthStencil.stencilTestEnable = VK_FALSE;
            depthStencil.front.passOp = vk::StencilOp::eKeep;
        }
    }

    template<typename VertexLayout>
    void SetVertexLayout()
    {
        auto const bindingDescriptions = VertexLayout::GetBindingDescriptions();
        vertexBindings.reserve(bindingDescriptions.size());
        for (auto const& description : bindingDescriptions)
            vertexBindings.emplace_back(description);

        auto const attributeDescriptions = VertexLayout::GetAttributeDescriptions();
        vertexAttributes.reserve(attributeDescriptions.size());
        for (auto const& description : attributeDescriptions)
            vertexAttributes.emplace_back(description);

        vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexBindings.size());
        vertexInputInfo.pVertexBindingDescriptions = vertexBindings.data();
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributes.size());
        vertexInputInfo.pVertexAttributeDescriptions = vertexAttributes.data();
    }

    void EnableColoredRefraction()
    {
        colorBlendAttachments.back().dstColorBlendFactor = vk::BlendFactor::eSrc1Color;
    }

    void SetFaceCullMode(vk::CullModeFlagBits mode)
    {
        rasterizer.cullMode = mode;
    }

    void SetAlphaBlendingPreMul(bool enable)
    {
        if (enable) {
            colorBlendAttachments.back().blendEnable = VK_TRUE;
            colorBlendAttachments.back().colorBlendOp = vk::BlendOp::eAdd;
            colorBlendAttachments.back().srcColorBlendFactor = vk::BlendFactor::eOne;
            colorBlendAttachments.back().dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
            ;
            colorBlendAttachments.back().alphaBlendOp = vk::BlendOp::eAdd;
            colorBlendAttachments.back().srcAlphaBlendFactor = vk::BlendFactor::eOne;
            colorBlendAttachments.back().dstAlphaBlendFactor = vk::BlendFactor::eZero;

        } else {
            colorBlendAttachments.back().blendEnable = VK_FALSE;
            colorBlendAttachments.back().srcColorBlendFactor = vk::BlendFactor::eZero;
            colorBlendAttachments.back().dstColorBlendFactor = vk::BlendFactor::eZero;
            colorBlendAttachments.back().alphaBlendOp = vk::BlendOp::eAdd;
            colorBlendAttachments.back().srcAlphaBlendFactor = vk::BlendFactor::eZero;
            colorBlendAttachments.back().dstAlphaBlendFactor = vk::BlendFactor::eZero;
        }
    }

    void SetAlphaBlending(bool enable)
    {
        if (enable) {
            colorBlendAttachments.back().blendEnable = VK_TRUE;
            colorBlendAttachments.back().colorBlendOp = vk::BlendOp::eAdd;
            colorBlendAttachments.back().srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
            colorBlendAttachments.back().dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
            ;
            colorBlendAttachments.back().alphaBlendOp = vk::BlendOp::eAdd;
            colorBlendAttachments.back().srcAlphaBlendFactor = vk::BlendFactor::eOne;
            colorBlendAttachments.back().dstAlphaBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;

        } else {
            colorBlendAttachments.back().blendEnable = VK_FALSE;
            colorBlendAttachments.back().srcColorBlendFactor = vk::BlendFactor::eZero;
            colorBlendAttachments.back().dstColorBlendFactor = vk::BlendFactor::eZero;
            colorBlendAttachments.back().alphaBlendOp = vk::BlendOp::eAdd;
            colorBlendAttachments.back().srcAlphaBlendFactor = vk::BlendFactor::eZero;
            colorBlendAttachments.back().dstAlphaBlendFactor = vk::BlendFactor::eZero;
        }
    }

    void SetMultiSampling(vk::SampleCountFlagBits sampleCount)
    {
        if (sampleCount == vk::SampleCountFlagBits::e1) {
            multiSampling.sampleShadingEnable = VK_FALSE;
            multiSampling.minSampleShading = 0.0f;
        } else {
            multiSampling.sampleShadingEnable = VK_TRUE;
            multiSampling.minSampleShading = 0.5f;
        }
        multiSampling.rasterizationSamples = sampleCount;
    }

    void SetDynamicState(vk::DynamicState dStateProperty, bool enable)
    {
        if (enable)
            dStateEnabled.emplace(dStateProperty);
        else
            dStateEnabled.erase(dStateProperty);
    }

    vk::UniquePipeline Build(vk::PipelineLayout pLayout, vk::RenderPass renderPass, uint32_t const subPassID, vk::PipelineCache pCache = {})
    {
        colorBlending.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
        colorBlending.pAttachments = colorBlendAttachments.data();

        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multiSampling;
        pipelineInfo.pDepthStencilState = &depthStencil;
        pipelineInfo.pColorBlendState = &colorBlending;

        pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        pipelineInfo.pStages = shaderStages.data();

        setDynamicState();

        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = subPassID;
        //            pipelineInfo.basePipelineHandle = basePipeline;
        pipelineInfo.layout = pLayout;

        return check(d.createGraphicsPipelineUnique(pCache, pipelineInfo));
    }

private:
    void setDynamicState()
    {
        auto const dStateSize = dStateEnabled.size();
        if (dStateSize == 0) {
            pipelineInfo.pDynamicState = nullptr;
            return;
        }

        dynamicStates.clear();
        dynamicStates.reserve(dStateSize);

        for (auto const& state : dStateEnabled)
            dynamicStates.emplace_back(state);

        dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(dStateSize);
        dynamicStateInfo.pDynamicStates = dynamicStates.data();
        pipelineInfo.pDynamicState = &dynamicStateInfo;
    }
};

class BuilderRayTracing {
    vk::Device d;

public:
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages {};

    explicit BuilderRayTracing(vk::Device d)
        : d(d)
    {
    }

    vk::UniquePipeline Build(vk::PipelineLayout pLayout, vk::PipelineCache pCache = {})
    {
        std::vector<vk::RayTracingShaderGroupCreateInfoKHR> shaderGroups;
        vk::RayTracingShaderGroupCreateInfoKHR group;

        for (size_t i = 0; i < shaderStages.size(); i++) {
            group.anyHitShader = vk::ShaderUnusedKHR;
            group.closestHitShader = vk::ShaderUnusedKHR;
            group.generalShader = vk::ShaderUnusedKHR;
            group.intersectionShader = vk::ShaderUnusedKHR;

            switch (shaderStages[i].stage) {
            case vk::ShaderStageFlagBits::eRaygenKHR:
            case vk::ShaderStageFlagBits::eMissKHR:
                group.type = vk::RayTracingShaderGroupTypeKHR::eGeneral;
                group.generalShader = static_cast<uint32_t>(i);
                break;
            case vk::ShaderStageFlagBits::eClosestHitKHR:
                group.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup;
                group.closestHitShader = static_cast<uint32_t>(i);
                break;
            default:
                break;
            }
            shaderGroups.push_back(group);
        }

        vk::RayTracingPipelineCreateInfoKHR rayPipelineInfo;
        rayPipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
        rayPipelineInfo.pStages = shaderStages.data();
        rayPipelineInfo.groupCount = static_cast<uint32_t>(shaderGroups.size());
        rayPipelineInfo.pGroups = shaderGroups.data();
        // recursion 2 to allow tracing shadow rays from hit shaders
        rayPipelineInfo.maxPipelineRayRecursionDepth = 2;
        rayPipelineInfo.layout = pLayout;

        auto result = check(d.createRayTracingPipelinesKHRUnique({}, pCache, { rayPipelineInfo }, nullptr));
        return std::move(result.back());
    }
};

}
