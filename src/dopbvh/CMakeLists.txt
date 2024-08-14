file(GLOB_RECURSE dopbvh_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

add_executable(dopbvh ${dopbvh_files})
target_compile_features(dopbvh PUBLIC cxx_std_23)

get_target_property(EXECUTABLE_NAME dopbvh NAME)
set_target_properties(
    dopbvh
        PROPERTIES
            OUTPUT_NAME_RELEASE ${EXECUTABLE_NAME}
            OUTPUT_NAME_DEBUG ${EXECUTABLE_NAME}_d
)

if (WIN32)
    if (MSVC)
        target_compile_options(dopbvh PRIVATE /W4 /bigobj /MP)
        set_target_properties(
            dopbvh
                PROPERTIES
                    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
        )
    endif()
else()
    target_compile_options(dopbvh PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_include_directories(
    dopbvh
        PRIVATE
            "${CMAKE_HOME_DIRECTORY}/data/shaders/"
)

target_link_libraries(
    dopbvh
        PRIVATE
            Taskflow
            glfw imgui assimp glm
            spdlog::spdlog
            tomlplusplus::tomlplusplus
            berries::berries
            vLime::vLime
            vk-radix-sort
)

target_compile_definitions(
    dopbvh
        PUBLIC
            DISABLE_SPDLOG_FMT_CONSTEVAL
            GLM_FORCE_DEPTH_ZERO_TO_ONE
)

set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE CACHE BOOL "" FORCE)
set(CMAKE_INSTALL_PREFIX "${CMAKE_HOME_DIRECTORY}/install" CACHE PATH "..." FORCE)
file(GLOB spv_files CONFIGURE_DEPENDS "${CMAKE_HOME_DIRECTORY}/data/shaders/*.spv")

install(FILES ${spv_files} DESTINATION data/shaders)
install(DIRECTORY ${CMAKE_HOME_DIRECTORY}/data/scene DESTINATION data/)
install(DIRECTORY ${CMAKE_HOME_DIRECTORY}/data/font DESTINATION data/)
install(DIRECTORY ${CMAKE_HOME_DIRECTORY}/data/image DESTINATION data/)
install(FILES ${CMAKE_HOME_DIRECTORY}/data/benchmark.toml ${CMAKE_HOME_DIRECTORY}/data/scene.toml DESTINATION data/)
install(FILES ${CMAKE_HOME_DIRECTORY}/data/imgui.ini DESTINATION .)
install(TARGETS dopbvh RUNTIME DESTINATION .)
