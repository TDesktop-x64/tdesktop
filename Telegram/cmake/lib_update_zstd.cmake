add_library(tdesktop_update_zstd INTERFACE)
add_library(tdesktop::update_zstd ALIAS tdesktop_update_zstd)

if (DESKTOP_APP_DISABLE_AUTOUPDATE)
    return()
endif()

if (DESKTOP_APP_USE_PACKAGED)
    find_package(PkgConfig QUIET)
    if (PkgConfig_FOUND)
        pkg_check_modules(UPDATE_ZSTD QUIET IMPORTED_TARGET libzstd>=1.5.0)
    endif()
    if (UPDATE_ZSTD_FOUND)
        target_link_libraries(tdesktop_update_zstd INTERFACE PkgConfig::UPDATE_ZSTD)
        return()
    endif()
endif()

include(FetchContent)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_DICTBUILDER OFF CACHE BOOL "" FORCE)
set(ZSTD_MULTITHREAD_SUPPORT ON CACHE BOOL "" FORCE)
if (MSVC)
    set(ZSTD_USE_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(update_zstd
    GIT_REPOSITORY https://github.com/facebook/zstd.git
    GIT_TAG ac66b19e6bd6b83238bf008eecc1298105298532
    SOURCE_SUBDIR build/cmake
)
block(SCOPE_FOR VARIABLES)
    # zstd 1.5.7 reads these optional variables under --warn-uninitialized.
    foreach(zstd_optional_variable CMAKE_RC_FLAGS LIBS_MT THREADS_LIBS)
        if (NOT DEFINED ${zstd_optional_variable})
            set(${zstd_optional_variable} "")
        endif()
    endforeach()
    # An empty can_fetch lets CMake 3.31's Git update script choose its default.
    if (NOT DEFINED can_fetch)
        set(can_fetch "")
    endif()
    FetchContent_MakeAvailable(update_zstd)
    if (CMAKE_C_COMPILER_ARG1 AND CMAKE_ASM_COMPILER STREQUAL CMAKE_C_COMPILER)
        # Compile assembly through the complete C command, including wrappers.
        set_source_files_properties(
            ${update_zstd_SOURCE_DIR}/lib/decompress/huf_decompress_amd64.S
            TARGET_DIRECTORY libzstd_static
            PROPERTIES LANGUAGE C
        )
    endif()
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
        # GNU assembly debug offsets can overflow in large DWARF64 links.
        set_property(SOURCE
            ${update_zstd_SOURCE_DIR}/lib/decompress/huf_decompress_amd64.S
            TARGET_DIRECTORY libzstd_static
            APPEND PROPERTY COMPILE_OPTIONS -g0
        )
    endif()
    target_include_directories(tdesktop_update_zstd SYSTEM
        INTERFACE ${update_zstd_SOURCE_DIR}/lib)
    target_link_libraries(tdesktop_update_zstd INTERFACE libzstd_static)
endblock()
