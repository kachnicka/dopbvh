#pragma once

#include <vLime/vLime.h>

namespace lime {
class MemoryManager;
class Transfer;
class ShaderCache;
}

namespace backend::vulkan {

struct VCtx {
    vk::Device d;
    vk::PhysicalDevice pd;

    lime::MemoryManager& memory;
    lime::Transfer& transfer;
    lime::ShaderCache& sCache;
};

}
