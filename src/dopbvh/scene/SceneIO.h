#pragma once

#include "../backend/data/Input.h"
#include "../backend/vulkan/Vulkan.h"
#include "Scene.h"
#include <assimp/Importer.hpp>
#include <assimp/ProgressHandler.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <berries/lib_helper/spdlog.h>
#include <glm/gtc/type_ptr.hpp>

class SceneIO {
    class Progress : public Assimp::ProgressHandler {
        f32* progress { nullptr };

    public:
        explicit Progress(f32* progress)
            : progress(progress)
        {
        }

        bool Update(f32 percentage) override
        {
            if (progress)
                *progress = percentage;
            return true;
        }
    };

public:
    Assimp::Importer importer;
    std::unique_ptr<Assimp::ProgressHandler> handler;

    void ImportScene(std::string_view path, f32* progress = nullptr)
    {
        handler = std::make_unique<Progress>(progress);
        importer.SetProgressHandler(handler.get());
        unsigned flags { static_cast<unsigned>(aiProcess_GenBoundingBoxes) };
        // flags |= aiProcess_FlipWindingOrder | aiProcess_CalcTangentSpace | aiProcess_GenSmoothNormals;
        flags |= aiProcess_Triangulate | aiProcess_GenNormals;
        flags |= aiProcess_JoinIdenticalVertices;
        // flags |= aiProcess_SortByPType;

        importer.ReadFile(path.data(), flags);
        importer.SetProgressHandler(nullptr);
    }

    // assimp: row-major
    // glm   : column-major
    inline static glm::mat4 matAssimpToGLM(aiMatrix4x4 mat)
    {
        return { *reinterpret_cast<glm::mat4*>(&mat.Transpose()) };
    }

    // Default assimp coordinate system:
    //  RHS, +X right, +Y up, +Z to the viewer
    // DopBVH coordinate system:
    //  RHS, +X right, +Y from the viewer, +Z up
    inline static void pointAssimpToDopBVHInPlace(aiVector3D& point, f32 scale = 1.f)
    {
        f32 tmp { -point.z };
        point.z = point.y;
        point.y = tmp;

        point *= scale;
    }
    static inline f32 GEOMETRY_GLOBAL_SCALE { 1.f };

    inline static Scene::AABB aabbAssimpToSceneNormalize(aiAABB const& aiAabb)
    {
        Scene::AABB result;
        result.min.x = std::min(aiAabb.mMin.x, aiAabb.mMax.x);
        result.min.y = std::min(aiAabb.mMin.y, aiAabb.mMax.y);
        result.min.z = std::min(aiAabb.mMin.z, aiAabb.mMax.z);
        result.max.x = std::max(aiAabb.mMax.x, aiAabb.mMin.x);
        result.max.y = std::max(aiAabb.mMax.y, aiAabb.mMin.y);
        result.max.z = std::max(aiAabb.mMax.z, aiAabb.mMin.z);
        return result;
    }

    inline static void ExpandAABB(Scene::AABB& toBeExpanded, Scene::AABB const& expander)
    {
        toBeExpanded.min.x = std::min(toBeExpanded.min.x, expander.min.x);
        toBeExpanded.min.y = std::min(toBeExpanded.min.y, expander.min.y);
        toBeExpanded.min.z = std::min(toBeExpanded.min.z, expander.min.z);
        toBeExpanded.max.x = std::max(toBeExpanded.max.x, expander.max.x);
        toBeExpanded.max.y = std::max(toBeExpanded.max.y, expander.max.y);
        toBeExpanded.max.z = std::max(toBeExpanded.max.z, expander.max.z);
    }

    template<typename PerNodeFunction>
    inline static void ProcessAssimpNodesBFS(aiNode* rootNode, PerNodeFunction f)
    {
        std::queue<aiNode*> queue;
        queue.push(rootNode);

        while (!queue.empty()) {
            auto* node { queue.front() };
            queue.pop();

            f(node);
            for (u32 i { 0 }; i < node->mNumChildren; i++)
                queue.push(node->mChildren[i]);
        }
    }

    struct {
        u32 getId()
        {
            return nextId++;
        }
        u32 reset()
        {
            return nextId = 0;
        }

    private:
        u32 nextId { 0 };
    } nodeIdGenerator;

    Scene CreateScene()
    {
        Scene result;
        nodeIdGenerator.reset();
        auto const* scene { importer.GetScene() };

        u32 nodeCount { 1 };
        ProcessAssimpNodesBFS(scene->mRootNode, [&nodeCount](aiNode* node) {
            nodeCount += node->mNumChildren;
        });

        result.nodes.resize(nodeCount);
        result.geometries.resize(scene->mNumMeshes);

        std::queue<u32> parents;
        ProcessAssimpNodesBFS(scene->mRootNode, [this, scene = &result, &parents](aiNode* aiNode) {
            auto const* aiScene { importer.GetScene() };
            auto const nodeId { nodeIdGenerator.getId() };
            auto& node { scene->nodes[nodeId] };

            node.id = nodeId;
            node.name = aiNode->mName.C_Str();
            node.transformLocal = matAssimpToGLM(aiNode->mTransformation);
            node.transformWorld = node.transformLocal;

            // create and reference all meshes
            node.geometry.resize(aiNode->mNumMeshes);
            for (u32 m = 0; m < aiNode->mNumMeshes; m++) {
                auto const& mesh { aiScene->mMeshes[aiNode->mMeshes[m]] };
                auto& g { scene->geometries[aiNode->mMeshes[m]] };
                node.geometry[m] = aiNode->mMeshes[m];

                if (g.id == Scene::INVALID_ID) {
                    g.id = aiNode->mMeshes[m];
                    g.name = mesh->mName.C_Str();

                    for (u32 i = 0; i < mesh->mNumVertices; i++) {
                        pointAssimpToDopBVHInPlace(mesh->mVertices[i], 10.f);
                        // pointAssimpToDopBVHInPlace(mesh->mVertices[i], GEOMETRY_GLOBAL_SCALE);
                        pointAssimpToDopBVHInPlace(mesh->mNormals[i]);
                    }

                    g.vertices.resize(mesh->mNumVertices);
                    g.normals.resize(mesh->mNumVertices);
                    memcpy(g.vertices.data(), mesh->mVertices, sizeof(aiVector3D) * g.vertices.size());
                    memcpy(g.normals.data(), mesh->mNormals, sizeof(aiVector3D) * g.normals.size());

                    g.indices.resize(mesh->mNumFaces * 3);
                    scene->triangleCount += mesh->mNumFaces;
                    for (u32 j { 0 }; j < mesh->mNumFaces; j++) {
                        g.indices[j * 3] = mesh->mFaces[j].mIndices[0];
                        g.indices[j * 3 + 1] = mesh->mFaces[j].mIndices[1];
                        g.indices[j * 3 + 2] = mesh->mFaces[j].mIndices[2];
                        g.surfaceArea += scene::triangleArea(g.vertices[g.indices[j * 3]], g.vertices[g.indices[j * 3 + 1]], g.vertices[g.indices[j * 3 + 2]]);
                    }

                    pointAssimpToDopBVHInPlace(mesh->mAABB.mMin, GEOMETRY_GLOBAL_SCALE);
                    pointAssimpToDopBVHInPlace(mesh->mAABB.mMax, GEOMETRY_GLOBAL_SCALE);
                    g.aabb = aabbAssimpToSceneNormalize(mesh->mAABB);
                    ExpandAABB(scene->aabb, g.aabb);
                    g.surfaceAreaToAabbRatio = g.surfaceArea / g.aabb.Area();
                }
            }

            // record the parent-children relationship
            if (!parents.empty())
                node.parent = parents.front();
            if (aiNode->mNumChildren > 0) {
                node.children.reserve(aiNode->mNumChildren);
                parents.push(nodeId);
            }
            if (node.parent != Scene::INVALID_ID) {
                scene->nodes[node.parent].children.emplace_back(nodeId);
                if (scene->nodes[node.parent].children.capacity() == scene->nodes[node.parent].children.size())
                    parents.pop();
            }
        });

        return result;
    }
};

static inline void UploadScene(Scene& scene, backend::vulkan::Vulkan& backend)
{
    backend.scenes.emplace_back();
    backend.scenes.back().data = &backend.deviceData;

    std::unordered_map<u32, backend::vulkan::data::ID_Geometry> geometryMap;

    for (auto const& geometry : scene.geometries) {
        backend::input::Geometry g {
            .vertexCount = static_cast<u32>(geometry.vertices.size()),
            .indexCount = static_cast<u32>(geometry.indices.size()),
            .vertexData = geometry.vertices.data(),
            .indexData = geometry.indices.data(),
            .uvData = nullptr,
            .normalData = geometry.normals.data(),
        };

        auto gId { backend.AddGeometry(g) };
        geometryMap[geometry.id] = gId;

        backend.scenes.back().geometries.emplace_back(gId);

        // TODO: for PLOC, this might have to be tracked per node, not per geometry
        backend.scenes.back().totalTriangleCount += g.indexCount / 3;
    }

    // upload aabbs
    backend.scenes.back().aabb = scene.aabb;
    struct PaddedAABB_TMP {
        glm::vec4 min {};
        glm::vec4 max {};
        glm::vec4 dynamicUniformPadding[2] {};
    };
    std::vector<PaddedAABB_TMP> aabbs;
    aabbs.reserve(scene.geometries.size());
    for (auto const& g : scene.geometries)
        aabbs.push_back({ .min = glm::vec4(g.aabb.min, 1.f), .max = glm::vec4(g.aabb.max, 1.f) });

    backend::input::Buffer aabbInput {
        .size = sizeof(PaddedAABB_TMP) * aabbs.size(),
        .data = aabbs.data(),
    };
    backend.scenes.back().addAABBs_TMP(aabbInput);

    aabbs.clear();
    aabbs.push_back({ .min = glm::vec4(scene.aabb.min, 1.f), .max = glm::vec4(scene.aabb.max, 1.f) });
    aabbInput.size = sizeof(PaddedAABB_TMP);
    aabbInput.data = aabbs.data();

    // update matrices
    scene.NodeDFS([&scene](Scene::Node& node) {
        node.transformWorld = node.transformLocal;
        if (node.parent != Scene::INVALID_ID)
            node.transformWorld = scene.nodes[node.parent].transformWorld * node.transformLocal;
    });

    struct WorldTransform {
        glm::mat4 toWorld;
        glm::mat4 toLocal;
    };

    std::vector<WorldTransform> transforms;
    transforms.reserve(scene.nodes.size());
    backend.scenes.back().nodeGeometry.reserve(scene.nodes.size());
    backend.scenes.back().localGeometryId.reserve(scene.nodes.size());
    backend.scenes.back().toWorld.reserve(scene.nodes.size());
    backend.scenes.back().sceneNodeToRenderedNodes.reserve(scene.nodes.size());
    for (auto const& node : scene.nodes) {
        backend.scenes.back().sceneNodeToRenderedNodes.emplace_back();
        for (auto const& gId : node.geometry) {
            backend.scenes.back().sceneNodeToRenderedNodes.back().emplace_back(gId);

            transforms.push_back({ .toWorld = node.transformWorld, .toLocal = glm::inverse(glm::transpose(node.transformWorld)) });
            backend.scenes.back().nodeGeometry.emplace_back(geometryMap[gId]);
            backend.scenes.back().localGeometryId.emplace_back(gId);
            // TODO: transpose for AS builder, might be problem if used elsewhere
            backend.scenes.back().toWorld.push_back(*reinterpret_cast<std::array<f32, 16> const*>(glm::value_ptr(glm::transpose(node.transformWorld))));
        }
    }

    backend::input::Buffer transformInput {
        .size = sizeof(WorldTransform) * transforms.size(),
        .data = transforms.data(),
    };

    backend.scenes.back().addTransforms_TMP(transformInput);
    backend.scenes.back().data->SetSceneDescription_TMP();
}

static inline void RecomputeMatrices(Scene& scene, backend::vulkan::Vulkan& backend)
{
    scene.NodeDFS([&scene](Scene::Node& node) {
        node.transformWorld = node.transformLocal;
        if (node.parent != Scene::INVALID_ID)
            node.transformWorld = scene.nodes[node.parent].transformWorld * node.transformLocal;
    });

    struct WorldTransform {
        glm::mat4 toWorld;
        glm::mat4 toLocal;
    };

    std::vector<WorldTransform> transforms;
    transforms.reserve(scene.nodes.size());

    backend.scenes.back().toWorld.clear();
    backend.scenes.back().toWorld.reserve(scene.nodes.size());

    for (auto const& node : scene.nodes) {
        for (auto const& gId : node.geometry) {
            static_cast<void>(gId);
            transforms.push_back({ .toWorld = node.transformWorld, .toLocal = glm::inverse(glm::transpose(node.transformWorld)) });
            backend.scenes.back().toWorld.push_back(*reinterpret_cast<std::array<f32, 16> const*>(glm::value_ptr(glm::transpose(node.transformWorld))));
        }
    }

    backend::input::Buffer transformInput {
        .size = sizeof(WorldTransform) * transforms.size(),
        .data = transforms.data(),
    };

    backend.scenes.back().addTransforms_TMP(transformInput);
}
