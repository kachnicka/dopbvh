#include "Serialization.h"

#include <fstream>
#include <iostream>
#include <vLime/types.h>

template<typename T>
inline static void write(std::ofstream& file, T const& data)
{
    file.write(reinterpret_cast<char const*>(&data), sizeof(T));
}

template<typename T>
inline static T read(std::ifstream& file)
{
    T data {};
    file.read(reinterpret_cast<char*>(&data), sizeof(T));
    return data;
}

template<>
inline void write(std::ofstream& file, Scene::Node const& data)
{
    write<u32>(file, data.id);
    // write<std::string>(file, data.name);
    write<u32>(file, data.parent);
    write<glm::mat4>(file, data.transformLocal);

    write<u32>(file, csize<u32>(data.children));
    for (auto child : data.children)
        write<u32>(file, child);

    write<u32>(file, csize<u32>(data.geometry));
    for (auto g : data.geometry)
        write<u32>(file, g);
}

template<>
inline Scene::Node read(std::ifstream& file)
{
    Scene::Node data;
    data.id = read<u32>(file);
    data.parent = read<u32>(file);
    data.transformLocal = read<glm::mat4>(file);
    data.transformWorld = data.transformLocal;

    u32 const childCount { read<u32>(file) };
    data.children.reserve(childCount);
    for (u32 i = 0; i < childCount; ++i)
        data.children.push_back(read<u32>(file));

    u32 const geometryCount { read<u32>(file) };
    data.geometry.reserve(geometryCount);
    for (u32 i = 0; i < geometryCount; ++i)
        data.geometry.push_back(read<u32>(file));

    return data;
}

template<>
inline void write(std::ofstream& file, Scene::Geometry const& data)
{
    write<u32>(file, data.id);
    // write<std::string>(file, data.name);
    write<Scene::AABB>(file, data.aabb);
    write<f32>(file, data.surfaceArea);
    write<f32>(file, data.surfaceAreaToAabbRatio);

    write<u32>(file, csize<u32>(data.vertices));
    for (auto v : data.vertices)
        write<glm::vec3>(file, v);

    write<u32>(file, csize<u32>(data.normals));
    for (auto n : data.normals)
        write<glm::vec3>(file, n);

    write<u32>(file, csize<u32>(data.indices));
    for (auto i : data.indices)
        write<u32>(file, i);
}

template<>
inline Scene::Geometry read(std::ifstream& file)
{
    Scene::Geometry data;
    data.id = read<u32>(file);
    data.aabb = read<Scene::AABB>(file);
    data.surfaceArea = read<f32>(file);
    data.surfaceAreaToAabbRatio = read<f32>(file);

    u32 const vertexCount { read<u32>(file) };
    data.vertices.reserve(vertexCount);
    for (u32 i = 0; i < vertexCount; ++i)
        data.vertices.push_back(read<glm::vec3>(file));

    u32 const normalCount { read<u32>(file) };
    data.normals.reserve(normalCount);
    for (u32 i = 0; i < normalCount; ++i)
        data.normals.push_back(read<glm::vec3>(file));

    u32 const indexCount { read<u32>(file) };
    data.indices.reserve(indexCount);
    for (u32 i = 0; i < indexCount; ++i)
        data.indices.push_back(read<u32>(file));

    return data;
}

namespace scene {

void serialize(Scene const& scene, std::filesystem::path const& dir)
{
    std::filesystem::path const path { dir / scene.path.stem().concat(".ob") };

    std::cout << "Scene serialization: " << path << "\n";

    std::ofstream file { path, std::ios::binary };
    if (!file.is_open())
        return;

    write<u32>(file, scene.triangleCount);
    write<Scene::AABB>(file, scene.aabb);

    write<size_t>(file, scene.nodes.size());
    for (auto const& node : scene.nodes)
        write<Scene::Node>(file, node);

    write<size_t>(file, scene.geometries.size());
    for (auto const& g : scene.geometries)
        write<Scene::Geometry>(file, g);

    file.close();
}

Scene deserialize(std::filesystem::path const& path)
{
    Scene result;

    std::ifstream file { path, std::ios::binary };
    if (!file.is_open())
        return result;

    result.triangleCount = read<u32>(file);
    result.aabb = read<Scene::AABB>(file);

    size_t const nodeCount { read<size_t>(file) };
    result.nodes.reserve(nodeCount);
    for (size_t i = 0; i < nodeCount; ++i)
        result.nodes.push_back(read<Scene::Node>(file));

    size_t const geometryCount { read<size_t>(file) };
    result.geometries.reserve(geometryCount);
    for (size_t i = 0; i < geometryCount; ++i)
        result.geometries.push_back(read<Scene::Geometry>(file));

    file.close();

    return result;
}

}
