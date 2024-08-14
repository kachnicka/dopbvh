add_subdirectory(dopbvh)

if (WIN32)
    set_directory_properties(
        PROPERTIES
            VS_STARTUP_PROJECT dopbvh
    )
endif()