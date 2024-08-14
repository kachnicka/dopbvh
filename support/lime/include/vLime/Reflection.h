#pragma once

#include <filesystem>
#include <string_view>
#include <unordered_map>
#include <vLime/PipelineBuilder.h>
#include <vLime/Util.h>
#include <vLime/Vulkan.h>
#include <vLime/types.h>

namespace lime {

struct ShaderLoaderSpv {
    std::vector<u32> spv;
    vk::ShaderStageFlagBits stage;

    explicit ShaderLoaderSpv(std::filesystem::path const& path);
};

class Shader {
    vk::UniqueShaderModule module;
    vk::ShaderStageFlagBits stage;

public:
    Shader(vk::Device d, ShaderLoaderSpv const& src)
    {
        vk::ShaderModuleCreateInfo cInfo {
            .codeSize = src.spv.size() * sizeof(u32),
            .pCode = src.spv.data()
        };
        module = check(d.createShaderModuleUnique(cInfo));
        stage = src.stage;
    }

    [[nodiscard]] vk::PipelineShaderStageCreateInfo GetStageCreateInfo(char const* entryPoint = "main") const
    {
        return {
            .stage = stage,
            .module = module.get(),
            .pName = entryPoint,
        };
    }
};

class ShaderCache {
    vk::UniquePipelineCache pCache;
    std::unordered_map<std::string, Shader> cache;

public:
    std::filesystem::path path;

    ShaderCache(vk::Device d, std::filesystem::path const& path)
        : pCache(createPipelineCache(d))
        , path(path)
    {
    }

    [[nodiscard]] vk::PipelineShaderStageCreateInfo getShaderCreateInfo(std::string_view shaderName, char const* entryPoint = "main") const
    {
        std::string key { shaderName };
        if (auto const m { cache.find(key) }; m != cache.cend())
            return m->second.GetStageCreateInfo(entryPoint);
        return {};
    }

    [[nodiscard]] vk::PipelineCache getPipelineCache() const
    {
        return pCache.get();
    }

    void loadShader(vk::Device d, ShaderLoaderSpv const& src, std::string_view shaderName)
    {
        std::string key { shaderName };
        cache.insert_or_assign(key, Shader { d, src });
    }

private:
    [[nodiscard]] vk::UniquePipelineCache createPipelineCache(vk::Device d) const
    {
        return check(d.createPipelineCacheUnique({}));
    }
};

struct PipelineLayout {
    vk::UniquePipelineLayout pipeline;
    std::vector<vk::UniqueDescriptorSetLayout> dSet;
};

class PipelineLayoutBuilder {
    std::vector<std::vector<vk::DescriptorSetLayoutBinding>> dsLayouts;
    std::unordered_map<vk::ShaderStageFlagBits, std::vector<vk::PushConstantRange>> pcRanges;

public:
    void ReflectSPV(std::vector<u32> spv, vk::ShaderStageFlagBits shaderStage);
    void AddDescriptorSetBinding(u32 dSetNum, vk::DescriptorSetLayoutBinding const& dsBinding);

    [[nodiscard]] PipelineLayout Build(vk::Device d) const;

    void AddPushConstantRange(vk::PushConstantRange const& pc, vk::ShaderStageFlagBits shaderStage)
    {
        pcRanges[shaderStage].emplace_back(pc);
    }

    void SetImmutableSamples(u8 dSetNum, uint8_t binding, vk::Sampler const* immutableSamplers)
    {
        dsLayouts[dSetNum][binding].pImmutableSamplers = immutableSamplers;
    }

    void SetDescriptorArraySize(u8 dSetNum, uint8_t binding, u32 size)
    {
        dsLayouts[dSetNum][binding].descriptorCount = size;
    }
};

template<typename Builder>
class Pipeline {
    ShaderCache& cache;
    std::vector<std::string_view> shaderNames;
    vk::UniquePipeline pipeline;

public:
    Builder pBuilder;
    PipelineLayout layout;

    [[nodiscard]] vk::Pipeline get() const
    {
        return pipeline.get();
    }

    [[nodiscard]] vk::PipelineLayout getLayout() const
    {
        return layout.pipeline.get();
    }

    Pipeline(vk::Device d, ShaderCache& cache, std::vector<std::string_view> _shaderNames)
        : cache(cache)
        , shaderNames(std::move(_shaderNames))
        , pBuilder(d)
    {
        PipelineLayoutBuilder lb;
        pBuilder.shaderStages.reserve(shaderNames.size());

        for (auto const& shaderName : shaderNames) {
            ShaderLoaderSpv src { cache.path / shaderName };
            cache.loadShader(d, src, shaderName);
            lb.ReflectSPV(std::move(src.spv), src.stage);
        }
        layout = lb.Build(d);
    }

    template<typename LayoutBuilderModifier>
    Pipeline(vk::Device d, ShaderCache& cache, std::vector<std::string_view> _shaderNames, LayoutBuilderModifier&& lbModify)
        : cache(cache)
        , shaderNames(std::move(_shaderNames))
        , pBuilder(d)
    {
        PipelineLayoutBuilder lb;
        pBuilder.shaderStages.reserve(shaderNames.size());

        for (auto const& shaderName : shaderNames) {
            ShaderLoaderSpv src { cache.path / shaderName };
            cache.loadShader(d, src, shaderName);
            lb.ReflectSPV(std::move(src.spv), src.stage);
        }
        std::forward<LayoutBuilderModifier>(lbModify)(lb);
        layout = lb.Build(d);
    }

    void build(vk::RenderPass renderPass = {}, u32 const subPassID = {})
    {
        pBuilder.shaderStages.clear();
        pBuilder.shaderStages.reserve(shaderNames.size());
        for (auto const& shaderName : shaderNames)
            pBuilder.shaderStages.emplace_back(cache.getShaderCreateInfo(shaderName));

        if constexpr (std::is_same_v<Builder, pipeline::BuilderGraphics>)
            pipeline = pBuilder.Build(layout.pipeline.get(), renderPass, subPassID, cache.getPipelineCache());
        if constexpr (std::is_same_v<Builder, pipeline::BuilderRayTracing>)
            pipeline = pBuilder.Build(layout.pipeline.get(), cache.getPipelineCache());
    }
};

using PipelineGraphics = Pipeline<pipeline::BuilderGraphics>;
using PipelineRayTracing = Pipeline<pipeline::BuilderRayTracing>;

}
