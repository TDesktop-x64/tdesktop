# This file is part of Telegram Desktop,
# the official desktop application for the Telegram messaging service.
#
# For license and copyright information please follow this link:
# https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL

add_library(lib_tgvoip INTERFACE IMPORTED GLOBAL)
add_library(tdesktop::lib_tgvoip ALIAS lib_tgvoip)

if (DESKTOP_APP_USE_PACKAGED)
    find_package(PkgConfig)
    if (PkgConfig_FOUND)
        pkg_check_modules(TGVOIP IMPORTED_TARGET tgvoip)
    endif()

    if (TGVOIP_FOUND)
        target_link_libraries(lib_tgvoip INTERFACE PkgConfig::TGVOIP)
        return()
    endif()
endif()

include(CMakeDependentOption)

add_library(lib_tgvoip_bundled STATIC)
init_target(lib_tgvoip_bundled)

cmake_dependent_option(LIBTGVOIP_DISABLE_ALSA "Disable libtgvoip's ALSA backend." OFF LINUX ON)
cmake_dependent_option(LIBTGVOIP_DISABLE_PULSEAUDIO "Disable libtgvoip's PulseAudio backend." OFF LINUX ON)

set(tgvoip_loc ${third_party_loc}/libtgvoip)

nice_target_sources(lib_tgvoip_bundled ${tgvoip_loc}
PRIVATE
    BlockingQueue.cpp
    BlockingQueue.h
    Buffers.cpp
    Buffers.h
    CongestionControl.cpp
    CongestionControl.h
    EchoCanceller.cpp
    EchoCanceller.h
    JitterBuffer.cpp
    JitterBuffer.h
    logging.cpp
    logging.h
    MediaStreamItf.cpp
    MediaStreamItf.h
    OpusDecoder.cpp
    OpusDecoder.h
    OpusEncoder.cpp
    OpusEncoder.h
    threading.h
    VoIPController.cpp
    VoIPGroupController.cpp
    VoIPController.h
    PrivateDefines.h
    VoIPServerConfig.cpp
    VoIPServerConfig.h
    audio/AudioInput.cpp
    audio/AudioInput.h
    audio/AudioOutput.cpp
    audio/AudioOutput.h
    audio/Resampler.cpp
    audio/Resampler.h
    NetworkSocket.cpp
    NetworkSocket.h
    PacketReassembler.cpp
    PacketReassembler.h
    MessageThread.cpp
    MessageThread.h
    audio/AudioIO.cpp
    audio/AudioIO.h
    video/ScreamCongestionController.cpp
    video/ScreamCongestionController.h
    video/VideoSource.cpp
    video/VideoSource.h
    video/VideoRenderer.cpp
    video/VideoRenderer.h
    json11.cpp
    json11.hpp

    # Windows
    os/windows/NetworkSocketWinsock.cpp
    os/windows/NetworkSocketWinsock.h
    os/windows/AudioInputWave.cpp
    os/windows/AudioInputWave.h
    os/windows/AudioOutputWave.cpp
    os/windows/AudioOutputWave.h
    os/windows/AudioOutputWASAPI.cpp
    os/windows/AudioOutputWASAPI.h
    os/windows/AudioInputWASAPI.cpp
    os/windows/AudioInputWASAPI.h
    os/windows/MinGWSupport.h
    os/windows/WindowsSpecific.cpp
    os/windows/WindowsSpecific.h

    # macOS
    os/darwin/AudioInputAudioUnit.cpp
    os/darwin/AudioInputAudioUnit.h
    os/darwin/AudioOutputAudioUnit.cpp
    os/darwin/AudioOutputAudioUnit.h
    os/darwin/AudioInputAudioUnitOSX.cpp
    os/darwin/AudioInputAudioUnitOSX.h
    os/darwin/AudioOutputAudioUnitOSX.cpp
    os/darwin/AudioOutputAudioUnitOSX.h
    os/darwin/AudioUnitIO.cpp
    os/darwin/AudioUnitIO.h
    os/darwin/DarwinSpecific.mm
    os/darwin/DarwinSpecific.h

    # Linux
    os/linux/AudioInputALSA.cpp
    os/linux/AudioInputALSA.h
    os/linux/AudioOutputALSA.cpp
    os/linux/AudioOutputALSA.h
    os/linux/AudioOutputPulse.cpp
    os/linux/AudioOutputPulse.h
    os/linux/AudioInputPulse.cpp
    os/linux/AudioInputPulse.h
    os/linux/AudioPulse.cpp
    os/linux/AudioPulse.h

    # POSIX
    os/posix/NetworkSocketPosix.cpp
    os/posix/NetworkSocketPosix.h
)

target_compile_definitions(lib_tgvoip_bundled
PRIVATE
    TGVOIP_USE_DESKTOP_DSP
)

target_include_directories(lib_tgvoip_bundled
PUBLIC
    ${tgvoip_loc}
)
target_link_libraries(lib_tgvoip_bundled
PRIVATE
    desktop-app::external_webrtc
    desktop-app::external_opus
)

if (APPLE)
    target_compile_definitions(lib_tgvoip_bundled
    PUBLIC
        TARGET_OS_OSX
        TARGET_OSX
    )
    if (build_macstore)
        target_compile_definitions(lib_tgvoip_bundled
        PUBLIC
            TGVOIP_NO_OSX_PRIVATE_API
        )
    endif()
elseif (LINUX)
    if (NOT LIBTGVOIP_DISABLE_ALSA)
        find_package(ALSA REQUIRED)
        target_include_directories(lib_tgvoip_bundled SYSTEM PRIVATE ${ALSA_INCLUDE_DIRS})
    else()
        remove_target_sources(lib_tgvoip_bundled ${tgvoip_loc}
            os/linux/AudioInputALSA.cpp
            os/linux/AudioInputALSA.h
            os/linux/AudioOutputALSA.cpp
            os/linux/AudioOutputALSA.h
        )

        target_compile_definitions(lib_tgvoip_bundled PRIVATE WITHOUT_ALSA)
    endif()

    if (NOT LIBTGVOIP_DISABLE_PULSEAUDIO)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(PULSE REQUIRED libpulse)
        target_include_directories(lib_tgvoip_bundled SYSTEM PRIVATE ${PULSE_INCLUDE_DIRS})
    else()
        remove_target_sources(lib_tgvoip_bundled ${tgvoip_loc}
            os/linux/AudioOutputPulse.cpp
            os/linux/AudioOutputPulse.h
            os/linux/AudioInputPulse.cpp
            os/linux/AudioInputPulse.h
            os/linux/AudioPulse.cpp
            os/linux/AudioPulse.h
        )

        target_compile_definitions(lib_tgvoip_bundled PRIVATE WITHOUT_PULSE)
    endif()
endif()

add_library(lib_tgvoip_bundled_options INTERFACE)

if (MSVC)
    target_compile_options(lib_tgvoip_bundled_options
    INTERFACE
        /wd4005 # 'identifier' : macro redefinition
        /wd4068 # unknown pragma
        /wd4996 # deprecated
        /wd5055 # operator '>' deprecated between enumerations and floating-point types
    )
else()
    target_compile_options_if_exists(lib_tgvoip_bundled_options
    INTERFACE
        -Wno-unqualified-std-cast-call
        -Wno-unused-variable
        -Wno-unknown-pragmas
        -Wno-error=sequence-point
        -Wno-error=unused-result
    )
    if (CMAKE_SIZEOF_VOID_P EQUAL 4 AND CMAKE_SYSTEM_PROCESSOR MATCHES "i686.*|i386.*|x86.*")
        target_compile_options(lib_tgvoip_bundled_options INTERFACE -msse2)
    endif()
endif()

target_link_libraries(lib_tgvoip_bundled
PRIVATE
    lib_tgvoip_bundled_options
)

target_link_libraries(lib_tgvoip
INTERFACE
    lib_tgvoip_bundled
)
