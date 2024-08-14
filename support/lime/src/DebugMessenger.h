#pragma once

#include <vLime/Vulkan.h>

namespace lime::debug {
vk::ResultValue<vk::UniqueDebugUtilsMessengerEXT> CreateDebugMessenger(vk::Instance i);
}
