set(imguiSourceFiles
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_demo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_tables.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/misc/cpp/imgui_stdlib.h
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/misc/cpp/imgui_stdlib.cpp

    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/misc/freetype/imgui_freetype.h
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/misc/freetype/imgui_freetype.cpp
)

add_library(imgui
    ${imguiSourceFiles}
)

target_link_libraries(imgui
    PRIVATE
        glfw
        freetype
)
target_compile_features(imgui PRIVATE cxx_std_17)

target_compile_definitions(
    imgui
        PUBLIC
            IMGUI_ENABLE_FREETYPE
)

target_include_directories(imgui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/imgui>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/imgui/backend>
)
