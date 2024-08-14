cmake_minimum_required(VERSION 3.19)

set(BERRIES_VERSION "0.0.1")
project(berries VERSION ${BERRIES_VERSION} LANGUAGES CXX DESCRIPTION "shared helpers")

file(GLOB_RECURSE IncludeHeader_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/include/berries/*.h")
file(GLOB_RECURSE Source_files CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(berries INTERFACE)
add_library(berries::berries ALIAS berries)

set_target_properties(berries PROPERTIES EXPORT_NAME berries)
target_compile_features(berries INTERFACE cxx_std_20)
target_include_directories(
    berries
        INTERFACE
            $<INSTALL_INTERFACE:include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_link_libraries(
    berries
        PRIVATE
)

target_compile_definitions(
    berries
        PUBLIC
)

if (WIN32)
    target_compile_options(berries INTERFACE /W4)
    target_compile_definitions(
        berries
            PUBLIC
    )
    target_link_libraries(berries PUBLIC)
else()
    target_compile_options(berries INTERFACE -Wall -Wextra -Wpedantic)
    target_compile_definitions(berries INTERFACE)
    target_link_libraries(
        berries
            INTERFACE
    )
endif()

if (WIN32)
#    set_target_properties(berries PROPERTIES)
endif()

return()

include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/berries)

install(
    TARGETS berries
    EXPORT berries-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

#Export the targets to a script
install(
    EXPORT berries-targets
    FILE berriesTargets.cmake
    NAMESPACE berries::
    DESTINATION ${INSTALL_CONFIGDIR}
)

#Create a ConfigVersion.cmake file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/berriesConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${CMAKE_CURRENT_LIST_DIR}/cmake/berriesConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/berriesConfig.cmake
    INSTALL_DESTINATION ${INSTALL_CONFIGDIR}
)

#Install the config, configversion and custom find modules
install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/berriesConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/berriesConfigVersion.cmake
        DESTINATION ${INSTALL_CONFIGDIR}
        )

##############################################
## Exporting from the build tree

export(
    EXPORT berries-targets
    FILE ${CMAKE_CURRENT_BINARY_DIR}/berriesTargets.cmake
    NAMESPACE berries::
)

#Register package in user's package registry
export(PACKAGE berries)
