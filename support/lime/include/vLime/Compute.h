#pragma once

#include <filesystem>

#include <string_view>
#include <vLime/Reflection.h>
#include <vLime/Vulkan.h>
#include <vLime/types.h>

namespace lime {
[[nodiscard]] constexpr static u32 divCeil(u32 a, u32 b)
{
    return (a + b - 1) / b;
}

class PipelineCompute {
    vk::UniquePipeline pipeline;

public:
    PipelineLayout layout;

    [[nodiscard]] vk::Pipeline get() const
    {
        return pipeline.get();
    }

    [[nodiscard]] vk::PipelineLayout getLayout() const
    {
        return layout.pipeline.get();
    }

    PipelineCompute() = default;
    PipelineCompute(vk::Device d, ShaderCache& cache, std::string_view shaderName, vk::SpecializationInfo const& specializationInfo = {})
    {
        ShaderLoaderSpv src { cache.path / shaderName };
        Shader shader { d, src };

        PipelineLayoutBuilder layoutBuilder;
        layoutBuilder.ReflectSPV(std::move(src.spv), src.stage);
        layout = layoutBuilder.Build(d);

        vk::ComputePipelineCreateInfo cInfo;
        cInfo.layout = layout.pipeline.get();
        cInfo.stage = shader.GetStageCreateInfo();

        if (specializationInfo.mapEntryCount > 0)
            cInfo.stage.pSpecializationInfo = &specializationInfo;

        pipeline = check(d.createComputePipelineUnique(cache.getPipelineCache(), cInfo));
    }
};

}
