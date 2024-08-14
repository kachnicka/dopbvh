cmake_minimum_required(VERSION 3.28)

set(VLIME_VERSION "0.0.1")
project(vLime VERSION ${VLIME_VERSION} LANGUAGES CXX DESCRIPTION "[v]ulkan [L]ayer [i]n the [m]iddl[e]")

find_package(Vulkan REQUIRED)
# Require Vulkan version ≥ 1.3.256 (earliest version when the Vulkan module was available)
if(${Vulkan_VERSION} VERSION_LESS "1.3.256")
    message(
        FATAL_ERROR "Minimum required Vulkan version for C++ modules is 1.3.256. "
        "Found ${Vulkan_VERSION}."
    )
endif()
add_subdirectory(extern)

option(LIME_VULKAN_AS_NAMED_MODULE "Build and link Vulkan as a cpp named module." OFF)

set(LIME_VULKAN_DEFINITIONS
    VULKAN_HPP_NO_EXCEPTIONS
    # VULKAN_HPP_NO_SMART_HANDLE
    VULKAN_HPP_NO_CONSTRUCTORS
    VULKAN_HPP_NO_SETTERS
    VULKAN_HPP_NO_TO_STRING
    VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1
    VULKAN_HPP_ASSERT_ON_RESULT=//
)

set(LIME_VULKAN_TARGET Vulkan::Headers)

# set up Vulkan C++ module as a library
if(LIME_VULKAN_AS_NAMED_MODULE)
    set(LIME_VULKAN_TARGET VulkanHppModule)

    add_library(VulkanHppModule)
    target_sources(VulkanHppModule
        PUBLIC
            FILE_SET CXX_MODULES
        BASE_DIRS
            ${Vulkan_INCLUDE_DIR}
        FILES
            ${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm
    )
    target_compile_features(
        VulkanHppModule
             PUBLIC
                 cxx_std_23
    )
    target_link_libraries(
        VulkanHppModule
            PUBLIC
                Vulkan::Headers
    )
    target_compile_definitions(
        VulkanHppModule
            PUBLIC
                ${LIME_VULKAN_DEFINITIONS}
    )
else()
    set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()
message("${LIME_VULKAN_TARGET}")

set(IncludeHeader_files
    include/vLime/Vulkan.h
    include/vLime/vLime.h

    include/vLime/Memory.h
    include/vLime/Transfer.h
    include/vLime/Capability.h
    include/vLime/DebugUtils.h
    include/vLime/Queues.h
    include/vLime/Reflection.h
    include/vLime/Compute.h
    include/vLime/ExtensionsAndLayers.h

    include/vLime/Frame.h
    include/vLime/CommandPool.h
    include/vLime/UtilImage.h
    include/vLime/RenderGraph.h
    include/vLime/PipelineBuilder.h

    include/vLime/Platform.h
    include/vLime/SwapChain.h

    include/vLime/Util.h
    include/vLime/types.h
    include/vLime/Flags.h
)
set(Source_files
    src/vLime.cpp
    src/Capability.cpp
    src/Util.cpp
    src/DebugMessenger.h
    src/DebugMessenger.cpp
    src/PhysicalDevice.h
    src/Reflection.cpp
)

add_library(
    vLime
        ${IncludeHeader_files}
        ${Source_files}
)
add_library(vLime::vLime ALIAS vLime)

set_target_properties(vLime PROPERTIES EXPORT_NAME vLime)
target_compile_features(vLime PUBLIC cxx_std_23)
target_include_directories(
    vLime
        PUBLIC
            $<INSTALL_INTERFACE:include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(
    vLime
        PUBLIC
            ${LIME_VULKAN_TARGET}
        PRIVATE
            spirv-cross-core
)

if(LIME_VULKAN_AS_NAMED_MODULE)
    target_compile_definitions(vLime PUBLIC LIME_VULKAN_AS_NAMED_MODULE)
else()
    target_compile_definitions(vLime PUBLIC ${LIME_VULKAN_DEFINITIONS})
endif()

if (WIN32)
    target_compile_options(vLime PRIVATE /W4)
    target_compile_definitions(
        vLime
            PUBLIC
                NOMINMAX
                VK_USE_PLATFORM_WIN32_KHR
                UNICODE _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    )
    target_link_libraries(vLime PUBLIC dxgi.lib)
else()
    target_compile_options(vLime PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_definitions(vLime PUBLIC VK_USE_PLATFORM_XLIB_KHR)
    target_link_libraries(
        vLime
            PRIVATE
                dl
                # Threads::Threads
    )
endif()

if (WIN32)
    set_target_properties(
        vLime
            PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
    )
endif()
