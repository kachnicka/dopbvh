#pragma once

#include <vLime/types.h>
#include <filesystem>
#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>
#include <queue>
#include <ranges>
#include <stack>
#include <string>
#include <unordered_map>
#include <vector>

namespace scene {

enum class Axis : u8 {
    eX,
    eY,
    eZ,
};

}

[[nodiscard]] inline static std::string to_string(scene::Axis axis)
{
    switch (axis) {
    case scene::Axis::eX:
        return "X";
    case scene::Axis::eY:
        return "Y";
    case scene::Axis::eZ:
        return "Z";
    default:
        return "None";
    }
}

struct Scene {
    inline static constexpr u32 INVALID_ID { std::numeric_limits<u32>::max() };

    // special tag for tree traversal with early sopping on closed nodes (e.g. traversal for gui)
    inline static constexpr u32 ON_CLOSE_TAG { INVALID_ID - 1 };

    struct AABB {
        glm::vec3 min { std::numeric_limits<f32>::max() };
        glm::vec3 max { std::numeric_limits<f32>::lowest() };

        [[nodiscard]] f32 Area() const
        {
            auto const d { max - min };
            return 2.f * (d.x * d.y + d.x * d.z + d.z * d.y);
        }

        [[nodiscard]] f32 Centroid(scene::Axis axis) const
        {
            switch (axis) {
            case scene::Axis::eX:
                return (min.x + max.x) * .5f;
            case scene::Axis::eY:
                return (min.y + max.y) * .5f;
            case scene::Axis::eZ:
                return (min.z + max.z) * .5f;
            }
            return 0.f;
        }

        [[nodiscard]] glm::vec3 Centroid() const
        {
            return (min + max) * .5f;
        }

        [[nodiscard]] AABB GetCubed() const
        {
            auto const diagonal { max - min };
            auto const maxEdge { std::max(std::max(diagonal.x, diagonal.y), diagonal.z) };
            return { .min = min, .max = min + maxEdge };
        }

        void Fit(glm::vec3 const& vertex)
        {
            min = glm::min(min, vertex);
            max = glm::max(max, vertex);
        }
    };

    struct Geometry {
        u32 id { INVALID_ID };
        std::string name;

        std::vector<glm::vec3> vertices;
        std::vector<glm::vec3> normals;
        std::vector<u32> indices;
        AABB aabb;
        f32 surfaceArea { 0.f };
        f32 surfaceAreaToAabbRatio { 0.f };
    };

    struct Node {
        u32 id { INVALID_ID };
        std::string name;
        u32 parent { INVALID_ID };

        glm::mat4 transformLocal { glm::identity<glm::mat4>() };
        glm::mat4 transformWorld { glm::identity<glm::mat4>() };

        std::vector<u32> geometry;
        std::vector<u32> children;
    };

    std::filesystem::path path;
    AABB aabb;
    std::vector<Geometry> geometries;
    std::vector<Node> nodes;
    u32 triangleCount { 0 };

    template<typename PerNodeFunction>
    void NodeBFS(PerNodeFunction f)
    {
        if (nodes.empty())
            return;

        std::queue<u32> queue;
        queue.push(0);

        while (!queue.empty()) {
            auto& node { nodes[queue.front()] };
            queue.pop();

            f(node);
            for (auto const& c : node.children)
                queue.push(c);
        }
    }

    template<typename PerNodeFunction>
    void NodeDFS(PerNodeFunction perNodeFunction)
    {
        if (nodes.empty())
            return;

        std::stack<u32> stack;
        stack.push(0);

        while (!stack.empty()) {
            auto& node { nodes[stack.top()] };
            stack.pop();

            perNodeFunction(node);

            for (auto const& child : std::ranges::reverse_view(node.children))
                stack.push(child);
        }
    }

    template<typename PerNodeFunction>
    void NodeDFS(PerNodeFunction perNodeFunction) const
    {
        if (nodes.empty())
            return;

        std::stack<u32> stack;
        stack.push(0);

        while (!stack.empty()) {
            auto& node { nodes[stack.top()] };
            stack.pop();

            perNodeFunction(node);

            for (auto const& child : std::ranges::reverse_view(node.children))
                stack.push(child);
        }
    }

    template<typename PerNodeFunction, typename OnNodeCloseFunction>
    void NodeDFS(PerNodeFunction perNodeFunction, OnNodeCloseFunction onNodeCloseFunction)
    {
        if (nodes.empty())
            return;

        std::stack<u32> stack;
        stack.push(0);

        while (!stack.empty()) {
            if (stack.top() == ON_CLOSE_TAG) {
                stack.pop();
                onNodeCloseFunction();
                continue;
            }

            auto& node { nodes[stack.top()] };
            stack.pop();
            if (auto const isOpen { perNodeFunction(node) }) {
                stack.push(ON_CLOSE_TAG);
                for (auto const& child : std::ranges::reverse_view(node.children))
                    stack.push(child);
            }
        }
    }

    f32 NodeOverlapSurfaceAreaToSceneAABBSurfaceArea() const
    {
        f32 result { 0.f };
        // local bounding boxes
        std::vector<AABB> worldAABBs;
        worldAABBs.reserve(geometries.size());

        auto const transformAABBToWorld { [](AABB localAABB, glm::mat4 const& w) -> AABB {
            AABB result;

            localAABB.min = glm::vec3(w * glm::vec4(localAABB.min, 1.f));
            localAABB.max = glm::vec3(w * glm::vec4(localAABB.max, 1.f));

            result.min.x = std::min(localAABB.min.x, localAABB.max.x);
            result.min.y = std::min(localAABB.min.y, localAABB.max.y);
            result.min.z = std::min(localAABB.min.z, localAABB.max.z);
            result.max.x = std::max(localAABB.min.x, localAABB.max.x);
            result.max.y = std::max(localAABB.min.y, localAABB.max.y);
            result.max.z = std::max(localAABB.min.z, localAABB.max.z);

            return result;
        } };

        auto const AABBOverlapSurfaceArea { [](AABB const& a, AABB const& b) -> f32 {
            f32 surfaceArea { 0.f };
            AABB overlap;
            overlap.min.x = std::max(a.min.x, b.min.x);
            overlap.min.y = std::max(a.min.y, b.min.y);
            overlap.min.z = std::max(a.min.z, b.min.z);
            overlap.max.x = std::min(a.max.x, b.max.x);
            overlap.max.y = std::min(a.max.y, b.max.y);
            overlap.max.z = std::min(a.max.z, b.max.z);

            if (overlap.min.x < overlap.max.x && overlap.min.y < overlap.max.y && overlap.min.z < overlap.max.z)
                surfaceArea = overlap.Area();
            return surfaceArea;
        } };

        AABB sceneAABB;
        for (auto const& node : nodes)
            for (auto gId : node.geometry) {
                worldAABBs.push_back(transformAABBToWorld(geometries[gId].aabb, node.transformWorld));
                sceneAABB.Fit(worldAABBs.back().min);
                sceneAABB.Fit(worldAABBs.back().max);
            }

        // traverse all AABB pairs
        for (size_t i { 0 }; i < worldAABBs.size() - 1; ++i)
            for (size_t j { i + 1 }; j < worldAABBs.size(); ++j)
                result += AABBOverlapSurfaceArea(worldAABBs[i], worldAABBs[j]);
        return result / sceneAABB.Area();
    }
};

namespace scene {

inline static f32 triangleArea(glm::vec3 const& A, glm::vec3 const& B, glm::vec3 const& C)
{
    auto const c { B - A };
    auto const b { C - A };
    auto const ccDot { glm::dot(c, c) };
    auto const bbDot { glm::dot(b, b) };
    auto const bcDot { glm::dot(b, c) };
    return .5f * sqrtf(ccDot * bbDot - bcDot * bcDot);
}

}
