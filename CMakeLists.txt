cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/cmake/convai-version.cmake")

project(ai_hardware_agent_examples
    VERSION "${CONVAI_PROJECT_VERSION_BASE}"
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CONVAI_PLATFORM "" CACHE STRING "Target platform")
set_property(CACHE CONVAI_PLATFORM PROPERTY STRINGS ws63 goldieos)

if(CONVAI_PLATFORM STREQUAL "esp32")
    message(FATAL_ERROR
        "ESP32-S3 uses its ESP-IDF entry point instead of the root CMake project.\n"
        "Run: cd examples/szpi-esp32s3 && idf.py set-target esp32s3 && idf.py build")
elseif(NOT CONVAI_PLATFORM MATCHES "^(ws63|goldieos)$")
    message(FATAL_ERROR
        "CONVAI_PLATFORM must be 'ws63' or 'goldieos'; got "
        "'${CONVAI_PLATFORM}'. Use one of the configure presets in "
        "CMakePresets.json.")
endif()

include("${CMAKE_CURRENT_LIST_DIR}/cmake/convai-build-options.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/convai-sdk.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/cmake/convai-artifacts.cmake")

if(CONVAI_PLATFORM STREQUAL "ws63")
    set(_convai_sdk_archive
        "${CMAKE_CURRENT_LIST_DIR}/libs/ws63/libconvai_sdk.a")
    set(_convai_platform_label "WS63 firmware")
else()
    set(_convai_sdk_archive
        "${CMAKE_CURRENT_LIST_DIR}/libs/win/libconvai_sdk.a")
    set(_convai_platform_label "GoldieOS Windows simulator")
endif()

convai_resolve_sdk_target(
    TARGET convai_sdk
    SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/src"
    SOURCE_BINARY_DIR "${CMAKE_BINARY_DIR}/src"
    PREBUILT_LIBRARY "${_convai_sdk_archive}"
    VERSION "${CONVAI_PROJECT_VERSION}"
    REQUIRED
    OUT_MODE _convai_sdk_mode
)

# Source-built SDK targets inherit project policy. Imported archives already
# contain their own compile settings and therefore only expose usage headers.
if(_convai_sdk_mode STREQUAL "SOURCE")
    convai_target_enable_warnings(convai_sdk)
    if(CONVAI_PLATFORM STREQUAL "ws63")
        convai_target_enable_ws63_options(convai_sdk)
    endif()
endif()

message(STATUS "CONVAI_PROJECT_VERSION = ${CONVAI_PROJECT_VERSION}")
message(STATUS
    "Configuring ${_convai_platform_label} (SDK mode: ${_convai_sdk_mode})")

add_subdirectory(examples/goldieos)
