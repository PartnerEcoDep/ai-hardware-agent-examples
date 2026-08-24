cmake_minimum_required(VERSION 3.21)

# ====================================================================
#  Default codec configuration for GoldieOS example
# ====================================================================
set(CONVAI_DEFAULT_CODEC "0" CACHE STRING "Default audio codec: 0=G.711A, 1=PCM16, 2=Opus, 3=G.711U, 4=IMA-ADPCM")
message(STATUS "CONVAI_DEFAULT_CODEC = ${CONVAI_DEFAULT_CODEC}")

# Propagate to SDK target if it exists
if(TARGET convai_sdk)
    target_compile_definitions(convai_sdk PRIVATE
        CONVAI_DEFAULT_CODEC=${CONVAI_DEFAULT_CODEC})
endif()

set(_goldieos_source_dir "${CMAKE_CURRENT_SOURCE_DIR}")
set(_goldieos_cmake_dir "${_goldieos_source_dir}/cmake")

include("${_goldieos_cmake_dir}/goldieos-common.cmake")
include("${_goldieos_cmake_dir}/goldieos-ws63.cmake")
include("${_goldieos_cmake_dir}/goldieos-win.cmake")
include("${_goldieos_cmake_dir}/ws63-mbedtls.cmake")
include("${_goldieos_cmake_dir}/ws63-firmware.cmake")

if(NOT TARGET convai_sdk)
    message(FATAL_ERROR
        "GoldieOS requires the convai_sdk target resolved by the root project")
endif()

goldieos_add_opus(
    PLATFORM "${CONVAI_PLATFORM}"
    SOURCE_DIR "${_goldieos_source_dir}"
)
if(NOT TARGET opus)
    message(FATAL_ERROR
        "Bundled Opus source is missing: "
        "${_goldieos_source_dir}/third_party/opus-1.6.1")
endif()

# The original directory-wide warning policy applied to Opus as well. Keep
# that behavior explicit now that all policies are target-scoped.
convai_target_enable_warnings(opus)

if(CONVAI_PLATFORM STREQUAL "ws63")
    convai_target_enable_ws63_options(opus)

    goldieos_add_ws63_target(
        TARGET goldieos_ws63
        PLATFORM_TARGET goldieos_ws63_platform
        SOURCE_DIR "${_goldieos_source_dir}"
    )
    goldieos_add_ws63_mbedtls(
        TARGET mbedtls_ws63
        SOURCE_DIR "${_goldieos_source_dir}"
        SDK_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include"
        PLATFORM_TARGET goldieos_ws63_platform
    )
    goldieos_add_ws63_firmware(
        TARGET ws63_firmware
        APP_TARGET goldieos_ws63
        OPUS_TARGET opus
        SDK_TARGET convai_sdk
        MBEDTLS_TARGET mbedtls_ws63
        DEFAULT_BOARD_ROOT "${_goldieos_source_dir}"
        BASENAME goldieos
        VERSION "${CONVAI_PROJECT_VERSION}"
    )

    message(STATUS
        "GoldieOS WS63 configured. Compiler: ${CMAKE_C_COMPILER}")
elseif(CONVAI_PLATFORM STREQUAL "goldieos")
    goldieos_add_win_target(
        TARGET goldieos
        SOURCE_DIR "${_goldieos_source_dir}"
        VERSION "${CONVAI_PROJECT_VERSION}"
    )
else()
    message(FATAL_ERROR
        "Unsupported GoldieOS platform: ${CONVAI_PLATFORM}")
endif()

