add_library(tdesktop_update_zstd INTERFACE)
add_library(tdesktop::update_zstd ALIAS tdesktop_update_zstd)

if (DESKTOP_APP_DISABLE_AUTOUPDATE)
    return()
endif()

if (DESKTOP_APP_USE_PACKAGED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(UPDATE_ZSTD REQUIRED IMPORTED_TARGET libzstd>=1.5.0)
    target_link_libraries(tdesktop_update_zstd INTERFACE PkgConfig::UPDATE_ZSTD)
else()
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
    FetchContent_MakeAvailable(update_zstd)
    target_include_directories(tdesktop_update_zstd SYSTEM
        INTERFACE ${update_zstd_SOURCE_DIR}/lib)
    target_link_libraries(tdesktop_update_zstd INTERFACE libzstd_static)
endif()
