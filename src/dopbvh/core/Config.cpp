#include "Config.h"

#define TOML_IMPLEMENTATION
#define TOML_HEADER_ONLY 0
#include <toml++/toml.hpp>

#include <iostream>

static toml::table tbl;
static std::vector<Config::Scene> scenesFromConfig;

void Config::ReadConfigFile(std::filesystem::path const& path, std::filesystem::path const& resources)
{
    try {
        tbl = toml::parse_file(path.c_str());
    } catch (toml::parse_error const& err) {
        std::cerr << "Parsing failed:\n"
                  << err << "\n";
    }

    auto pathPrefixScene { tbl["path_prefix_scene"].value<std::string_view>().value() };
    auto pathPrefixCamera { tbl["path_prefix_camera"].value<std::string_view>().value() };

    auto resScene { (resources / pathPrefixScene).generic_string() };
    auto resCamera { (resources / pathPrefixCamera).generic_string() };

    if (!std::filesystem::path(pathPrefixScene).is_absolute())
        pathPrefixScene = resScene;
    if (!std::filesystem::path(pathPrefixCamera).is_absolute())
        pathPrefixCamera = resCamera;

    auto* scenes { tbl["scenes"].as_array() };
    for (auto& scene : *scenes) {
        auto& s { *scene.as_table() };
        Config::Scene config;
        if (auto name { s["name"].value<std::string_view>() }; name) {
            config.name = std::string(name.value());
            if (auto file { s["file"].value<std::string_view>() }; file && !file->empty())
                config.path = std::string(pathPrefixScene).append(file.value());
            if (auto cam { s["cam"].value<std::string_view>() }; cam && !cam->empty())
                config.camera = std::string(pathPrefixCamera).append(cam.value());
            if (auto file { s["bin"].value<std::string_view>() }; file && !file->empty())
                config.bin = std::string(pathPrefixScene).append(file.value());
            scenesFromConfig.push_back(config);
        }
    }
}

std::vector<Config::Scene>& Config::GetScenes()
{
    return scenesFromConfig;
}

Config::Scene Config::GetScene(std::string_view sceneName)
{
    Scene result;

    if (sceneName.empty()) {
        auto startupScene { tbl["startup_scene"].value<std::string_view>() };
        if (!startupScene)
            return {};
        sceneName = startupScene.value();
    }

    for (auto& scene : scenesFromConfig)
        if (scene.name == sceneName)
            return scene;

    return {};
}

backend::config::BV getBoundingVolume(std::string_view bv)
{
    if (bv == "aabb")
        return backend::config::BV::eAABB;
    if (bv == "dop14")
        return backend::config::BV::eDOP14;
    if (bv == "obb")
        return backend::config::BV::eOBB;
    return backend::config::BV::eNone;
}

backend::config::CompressedLayout getCompressedLayout(std::string_view layout)
{
    if (layout == "binary_standard")
        return backend::config::CompressedLayout::eBinaryStandard;
    if (layout == "binary_dop14_split")
        return backend::config::CompressedLayout::eBinaryDOP14Split;
    return backend::config::CompressedLayout::eBinaryStandard;
}

backend::config::SpaceFilling getSFC(std::string_view sfc)
{
    if (sfc == "morton32")
        return backend::config::SpaceFilling::eMorton32;
    return backend::config::SpaceFilling::eMorton32;
}

void getPipeline(toml::table const& table, backend::config::BVHPipeline& pipeline)
{
    if (auto const value { table.at_path("plocpp.bv").value<std::string_view>() }; value)
        pipeline.plocpp.bv = getBoundingVolume(value.value());
    if (auto const value { table.at_path("plocpp.space_filling").value<std::string_view>() }; value)
        pipeline.plocpp.sfc = getSFC(value.value());
    if (auto const value { table.at_path("plocpp.radius").value<u32>() }; value)
        pipeline.plocpp.radius = value.value();

    if (auto const value { table.at_path("collapsing.bv").value<std::string_view>() }; value)
        pipeline.collapsing.bv = getBoundingVolume(value.value());
    if (auto const value { table.at_path("collapsing.max_leaf_size").value<u32>() }; value)
        pipeline.collapsing.maxLeafSize = value.value();
    if (auto const value { table.at_path("collapsing.c_t").value<f32>() }; value)
        pipeline.collapsing.c_t = value.value();
    if (auto const value { table.at_path("collapsing.c_i").value<f32>() }; value)
        pipeline.collapsing.c_i = value.value();

    if (auto const value { table.at_path("transformation.bv").value<std::string_view>() }; value)
        pipeline.transformation.bv = getBoundingVolume(value.value());

    if (auto const value { table.at_path("compression.bv").value<std::string_view>() }; value)
        pipeline.compression.bv = getBoundingVolume(value.value());
    if (auto const value { table.at_path("compression.layout").value<std::string_view>() }; value)
        pipeline.compression.layout = getCompressedLayout(value.value());

    if (auto const value { table.at_path("stats.c_t").value<f32>() }; value)
        pipeline.stats.c_t = value.value();
    if (auto const value { table.at_path("stats.c_i").value<f32>() }; value)
        pipeline.stats.c_i = value.value();

    if (auto const value { table.at_path("tracer.bv").value<std::string_view>() }; value)
        pipeline.tracer.bv = getBoundingVolume(value.value());
    if (auto const value { table.at_path("tracer.workgroup_count").value<u32>() }; value)
        pipeline.tracer.workgroupCount = value.value();
    // if (auto const value { table.at_path("tracer.trace_bv").value<bool>() }; value)
    //     pipeline.tracer.traceBoundingVolumes = value.value();
    if (auto const value { table.at_path("tracer.trace_mode").value<i32>() }; value)
        pipeline.tracer.traceMode = value.value();
    if (auto const value { table.at_path("tracer.use_separate_kernels").value<bool>() }; value)
        pipeline.tracer.useSeparateKernels = value.value();
}

backend::config::BVHPipeline getPipeline(toml::table const& table)
{
    backend::config::BVHPipeline pipeline;
    getPipeline(table, pipeline);
    return pipeline;
}

backend::config::BVHPipeline getParentPipeline(std::optional<std::string_view> parentName, std::vector<backend::config::BVHPipeline> const& pipelines, backend::config::BVHPipeline const& defaultPipeline)
{
    if (!parentName)
        return defaultPipeline;

    for (auto const& p : pipelines) {
        if (p.name == parentName)
            return p;
    }
    return defaultPipeline;
}

std::vector<backend::config::BVHPipeline> Config::GetBVHPipelines(std::filesystem::path const& benchmark, std::vector<std::string>& scene, std::vector<std::string>& method)
{
    std::vector<backend::config::BVHPipeline> result;

    toml::table cfg;
    try {
        cfg = toml::parse_file(benchmark.c_str());
    } catch (toml::parse_error const& err) {
        std::cerr << "Parsing failed:\n"
                  << err << "\n";
    }

    auto const defaultPipeline { getPipeline(*cfg["default"].as_table()) };

    if (auto const value { cfg["default"]["benchmark_scenes"].as_array() })
        for (auto& v : *value)
            scene.push_back(v.as_string()->get());
    if (auto const value { cfg["default"]["benchmark_config"].as_array() })
        for (auto& v : *value)
            method.push_back(v.as_string()->get());

    auto* benchmarks { cfg["benchmark"].as_array() };
    if (!benchmarks)
        return result;

    for (auto& b : *benchmarks) {
        auto const name { b.as_table()->at("name").value<std::string_view>() };
        if (!name)
            continue;

        auto const parentName { b.as_table()->at_path("parent").value<std::string_view>() };
        result.emplace_back(getParentPipeline(parentName, result, defaultPipeline));
        result.back().name = std::string(name.value());
        getPipeline(*b.as_table(), result.back());
    }

    return result;
}
