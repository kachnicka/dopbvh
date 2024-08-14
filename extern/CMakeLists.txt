add_subdirectory(spdlog EXCLUDE_FROM_ALL)

set(TF_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TF_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(glm EXCLUDE_FROM_ALL)
add_subdirectory(taskflow EXCLUDE_FROM_ALL)

unset(BUILD_SHARED_LIBS CACHE)
set(ZLIB_COMPAT ON CACHE BOOL "" FORCE)
set(ZLIB_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(ZLIBNG_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(zlib-ng EXCLUDE_FROM_ALL)
# assimp and freetype both don't handle zlib as subdirectory in a nice way, at least for static linking
# to avoid maintating custom cmake, fill manually variables a find_package() would fill
set(ZLIB_FOUND TRUE)
SET(ZLIB_LIBRARY zlibstatic)
SET(ZLIB_LIBRARIES zlibstatic)
get_target_property(ZLIB_INCLUDE_DIR zlibstatic INCLUDE_DIRECTORIES)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL_PDB OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(assimp EXCLUDE_FROM_ALL)

# GCC12: remove forced -Werror parameter inside of assimp preventing build
get_target_property(ASSIMP_COMPILE_FLAGS assimp COMPILE_OPTIONS )
list(REMOVE_ITEM ASSIMP_COMPILE_FLAGS -Werror)
set_target_properties(assimp PROPERTIES COMPILE_OPTIONS "${ASSIMP_COMPILE_FLAGS}")
# MSVC: allow linking of "import library" for tests build
target_compile_definitions(assimp PRIVATE ASSIMP_BUILD_DLL_EXPORT)

# skip search for system library, zlib target will consume manually set subdirectory
set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)
set(SKIP_INSTALL_ALL ON CACHE BOOL "" FORCE)
add_subdirectory(freetype EXCLUDE_FROM_ALL)
add_subdirectory(imgui EXCLUDE_FROM_ALL)

set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(glfw EXCLUDE_FROM_ALL)

add_subdirectory(fuchsia_radix_sort EXCLUDE_FROM_ALL)
add_subdirectory(tomlplusplus EXCLUDE_FROM_ALL)
