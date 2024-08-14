#include "../include/vLime/Reflection.h"

#include <filesystem>
#include <format>
#include <fstream>
#include <spirv_common.hpp>
#include <spirv_reflect.hpp>
#include <vLime/Util.h>

namespace lime {

[[nodiscard]] std::vector<u32> readSpv(std::filesystem::path const& path)
{
    std::ifstream file { path, std::ios::ate | std::ios::binary };

    if (!file.is_open()) {
        log::error(std::format("Failed to open file '{}'!", path.generic_string()));
        abort();
    }

    constexpr auto factor = sizeof(u32) / sizeof(char);
    auto const fileSizeByte = static_cast<size_t>(file.tellg());
    auto const fileSizeUint = fileSizeByte / factor;

    std::vector<u32> buffer;
    buffer.reserve(fileSizeUint);

    file.seekg(0);

    u32 val;
    for (size_t i = 0; i < fileSizeUint; i++) {
        file.read(reinterpret_cast<char*>(&val), sizeof(val));
        buffer.emplace_back(val);
    }
    file.close();

    return buffer;
}

[[nodiscard]] vk::ShaderStageFlagBits stageNameToStageFlag(std::string_view fileExtension)
{
    if (fileExtension == ".vert")
        return vk::ShaderStageFlagBits::eVertex;
    if (fileExtension == ".tesc")
        return vk::ShaderStageFlagBits::eTessellationControl;
    if (fileExtension == ".tese")
        return vk::ShaderStageFlagBits::eTessellationEvaluation;
    if (fileExtension == ".geom")
        return vk::ShaderStageFlagBits::eGeometry;
    if (fileExtension == ".frag")
        return vk::ShaderStageFlagBits::eFragment;
    if (fileExtension == ".comp")
        return vk::ShaderStageFlagBits::eCompute;
    if (fileExtension == ".rgen")
        return vk::ShaderStageFlagBits::eRaygenKHR;
    if (fileExtension == ".rint")
        return vk::ShaderStageFlagBits::eIntersectionKHR;
    if (fileExtension == ".rahit")
        return vk::ShaderStageFlagBits::eAnyHitKHR;
    if (fileExtension == ".rchit")
        return vk::ShaderStageFlagBits::eClosestHitKHR;
    if (fileExtension == ".rmiss")
        return vk::ShaderStageFlagBits::eMissKHR;
    if (fileExtension == ".rcall")
        return vk::ShaderStageFlagBits::eCallableKHR;
    if (fileExtension == ".mesh")
        return vk::ShaderStageFlagBits::eMeshNV;
    if (fileExtension == ".task")
        return vk::ShaderStageFlagBits::eTaskNV;

    return vk::ShaderStageFlagBits::eAll;
}

ShaderLoaderSpv::ShaderLoaderSpv(std::filesystem::path const& path)
    : spv { readSpv(path) }
    , stage { stageNameToStageFlag(path.stem().extension().string()) }
{
}

void PipelineLayoutBuilder::ReflectSPV(std::vector<uint32_t> spv, vk::ShaderStageFlagBits shaderStage)
{
    spirv_cross::Compiler const comp { std::move(spv) };
    spirv_cross::ShaderResources const resources { comp.get_shader_resources() };

    vk::DescriptorSetLayoutBinding binding;
    binding.stageFlags = shaderStage;

    for (auto const& r : resources.uniform_buffers) {
        binding.descriptorType = vk::DescriptorType::eUniformBufferDynamic;
        binding.descriptorCount = 1;
        binding.binding = comp.get_decoration(r.id, spv::DecorationBinding);
        AddDescriptorSetBinding(comp.get_decoration(r.id, spv::DecorationDescriptorSet), binding);
    }

    for (auto const& r : resources.storage_buffers) {
        binding.descriptorType = vk::DescriptorType::eStorageBufferDynamic;
        binding.descriptorCount = 1;
        binding.binding = comp.get_decoration(r.id, spv::DecorationBinding);
        AddDescriptorSetBinding(comp.get_decoration(r.id, spv::DecorationDescriptorSet), binding);
    }

    for (auto const& r : resources.sampled_images) {
        binding.descriptorType = vk::DescriptorType::eCombinedImageSampler;
        binding.descriptorCount = 1;
        binding.binding = comp.get_decoration(r.id, spv::DecorationBinding);
        AddDescriptorSetBinding(comp.get_decoration(r.id, spv::DecorationDescriptorSet), binding);
    }

    for (auto const& r : resources.storage_images) {
        binding.descriptorType = vk::DescriptorType::eStorageImage;
        binding.descriptorCount = 1;
        binding.binding = comp.get_decoration(r.id, spv::DecorationBinding);
        AddDescriptorSetBinding(comp.get_decoration(r.id, spv::DecorationDescriptorSet), binding);
    }

    for (auto const& r : resources.acceleration_structures) {
        binding.descriptorType = vk::DescriptorType::eAccelerationStructureKHR;
        binding.descriptorCount = 1;
        binding.binding = comp.get_decoration(r.id, spv::DecorationBinding);
        AddDescriptorSetBinding(comp.get_decoration(r.id, spv::DecorationDescriptorSet), binding);
    }

    for (auto const& r : resources.push_constant_buffers) {
        vk::PushConstantRange pc;
        pc.stageFlags = shaderStage;

        auto& type = comp.get_type(r.base_type_id);
        for (u32 i = 0; i < type.member_types.size(); i++) {
            pc.size = static_cast<uint32_t>(comp.get_declared_struct_member_size(type, i));
            pc.offset = comp.type_struct_member_offset(type, i);
            AddPushConstantRange(pc, shaderStage);
        }
    }
}

void PipelineLayoutBuilder::AddDescriptorSetBinding(u32 dSetNum, vk::DescriptorSetLayoutBinding const& dsBinding)
{
    if (dSetNum >= dsLayouts.size())
        dsLayouts.resize(dSetNum + 1);

    auto& dsLayoutBindings { dsLayouts[dSetNum] };
    if (dsBinding.binding >= dsLayoutBindings.size())
        dsLayoutBindings.resize(dsBinding.binding + 1);

    auto& dsLayoutBinding { dsLayoutBindings[dsBinding.binding] };
    if (dsLayoutBinding.descriptorCount == 0)
        dsLayoutBinding = dsBinding;
    else
        dsLayoutBinding.stageFlags |= dsBinding.stageFlags;
}

PipelineLayout PipelineLayoutBuilder::Build(vk::Device d) const
{
    PipelineLayout result;

    result.dSet.reserve(dsLayouts.size());
    for (auto const& dsLayout : dsLayouts) {
        vk::DescriptorSetLayoutCreateInfo cInfo;
        cInfo.bindingCount = csize<u32>(dsLayout);
        cInfo.pBindings = dsLayout.data();
        result.dSet.emplace_back(check(d.createDescriptorSetLayoutUnique(cInfo)));
    }

    std::vector<vk::DescriptorSetLayout> pSetLayouts;
    pSetLayouts.reserve(result.dSet.size());
    for (auto const& l : result.dSet)
        pSetLayouts.emplace_back(l.get());

    vk::PipelineLayoutCreateInfo cInfo;
    cInfo.setLayoutCount = csize<u32>(pSetLayouts);
    cInfo.pSetLayouts = pSetLayouts.data();

    std::vector<vk::PushConstantRange> pushConstants;
    if (!pcRanges.empty()) {
        for (auto const& [stage, stagePcs] : pcRanges) {
            u32 beginAddress = std::numeric_limits<u32>::max();
            u32 endAddress = std::numeric_limits<u32>::min();
            for (auto const& [stageFlags, offset, size] : stagePcs) {
                beginAddress = std::min(beginAddress, offset);
                endAddress = std::max(endAddress, offset + size);
            }
            pushConstants.emplace_back(stage, beginAddress, endAddress - beginAddress);
        }
        cInfo.pushConstantRangeCount = csize<u32>(pushConstants);
        cInfo.pPushConstantRanges = pushConstants.data();
    }

    result.pipeline = check(d.createPipelineLayoutUnique(cInfo));
    return result;
}

}
