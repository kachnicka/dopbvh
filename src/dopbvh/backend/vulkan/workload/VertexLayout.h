#pragma once

#include <vLime/vLime.h>

namespace backend::vulkan::vertex_layout {
struct Empty {
    static std::array<vk::VertexInputBindingDescription, 0> GetBindingDescriptions()
    {
        return {};
    }

    static std::array<vk::VertexInputAttributeDescription, 0> GetAttributeDescriptions()
    {
        return {};
    }
};

struct ImGUI {
    static inline std::array<vk::VertexInputBindingDescription, 1> GetBindingDescriptions()
    {
        std::array<vk::VertexInputBindingDescription, 1> bindingDescriptions;
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = 5 * sizeof(float);
        bindingDescriptions[0].inputRate = vk::VertexInputRate::eVertex;

        return bindingDescriptions;
    }

    static inline std::array<vk::VertexInputAttributeDescription, 3> GetAttributeDescriptions()
    {
        std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions;

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[0].offset = 0 * sizeof(float);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32Sfloat;
        attributeDescriptions[1].offset = 2 * sizeof(float);

        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = vk::Format::eR8G8B8A8Unorm;
        attributeDescriptions[2].offset = 4 * sizeof(float);

        return attributeDescriptions;
    }
};

struct Scene {
    static inline auto GetBindingDescriptions()
    {
        std::array<vk::VertexInputBindingDescription, 2> bindingDescriptions;
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = 3 * sizeof(float);
        bindingDescriptions[0].inputRate = vk::VertexInputRate::eVertex;

        bindingDescriptions[1].binding = 1;
        bindingDescriptions[1].stride = 3 * sizeof(float);
        bindingDescriptions[1].inputRate = vk::VertexInputRate::eVertex;

        return bindingDescriptions;
    }

    static inline auto GetAttributeDescriptions()
    {
        std::array<vk::VertexInputAttributeDescription, 2> attributeDescriptions;

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = 0 * sizeof(float);

        attributeDescriptions[1].binding = 1;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = 0 * sizeof(float);

        return attributeDescriptions;
    }
};

struct DopVis {
    static inline auto GetBindingDescriptions()
    {
        std::array<vk::VertexInputBindingDescription, 3> bindingDescriptions;
        bindingDescriptions[0].binding = 0;
        bindingDescriptions[0].stride = 3 * sizeof(float);
        bindingDescriptions[0].inputRate = vk::VertexInputRate::eVertex;

        bindingDescriptions[1].binding = 1;
        bindingDescriptions[1].stride = 3 * sizeof(float);
        bindingDescriptions[1].inputRate = vk::VertexInputRate::eVertex;

        bindingDescriptions[2].binding = 2;
        bindingDescriptions[2].stride = 1 * sizeof(uint8_t);
        bindingDescriptions[2].inputRate = vk::VertexInputRate::eVertex;

        return bindingDescriptions;
    }

    static inline auto GetAttributeDescriptions()
    {
        std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions;

        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[0].offset = 0 * sizeof(float);

        attributeDescriptions[1].binding = 1;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;
        attributeDescriptions[1].offset = 0 * sizeof(float);

        attributeDescriptions[2].binding = 2;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = vk::Format::eR8Uint;
        attributeDescriptions[2].offset = 0 * sizeof(float);

        return attributeDescriptions;
    }
};

}
