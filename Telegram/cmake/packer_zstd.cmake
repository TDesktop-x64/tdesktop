add_executable(Packer_zstd)
init_target(Packer_zstd)
add_dependencies(Telegram Packer_zstd)

target_include_directories(Packer_zstd PRIVATE ${src_loc})
nice_target_sources(Packer_zstd ${src_loc}
PRIVATE
    _other/packer_zstd.cpp
    _other/packer_zstd.h
    core/update_zstd.cpp
    core/update_zstd.h
    core/update_zstd_pack.cpp
    core/update_zstd_pack.h
)
target_link_libraries(Packer_zstd
PRIVATE
    desktop-app::external_qt
    desktop-app::external_openssl
    tdesktop::update_zstd
)
if (WIN32 AND DESKTOP_APP_WIN7_BASELINE)
    target_link_libraries(Packer_zstd PRIVATE desktop-app::lib_base)
    target_link_options(Packer_zstd PRIVATE /SUBSYSTEM:CONSOLE,6.01)
endif()
set_target_properties(Packer_zstd PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${output_folder})
