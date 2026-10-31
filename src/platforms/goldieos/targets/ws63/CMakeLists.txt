cmake_minimum_required(VERSION 3.21)
# The vendor .bat launcher is subject to cmd.exe's command-line length limit.
set(CMAKE_NINJA_FORCE_RESPONSE_FILE ON)
project(unified_ai_ws63 C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)
get_filename_component(UNIFIED_APP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
set(UNIFIED_SOURCE_ROOT "${UNIFIED_APP_ROOT}/src")
set(app_root "${UNIFIED_APP_ROOT}")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/runtime.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/board.cmake")
set(CONVAI_SDK_ROOT "${app_root}/third_party/convai-sdk-26.8.0" CACHE PATH "ConvAI SDK distribution")
set(WS63_LOCAL_CONFIG "${app_root}/config/ws63.local.h" CACHE FILEPATH "Private WS63 credentials")
set(sdk_include "${CONVAI_SDK_ROOT}/include")
if(NOT EXISTS "${CONVAI_SDK_ROOT}/libs/ws63/libconvai_sdk.a")
    message(FATAL_ERROR "Missing WS63 SDK binary")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/convai-build-options.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/convai-artifacts.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/goldieos-common.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/goldieos-ws63.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/ws63-mbedtls.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/ws63-firmware.cmake")
add_library(convai_sdk STATIC IMPORTED GLOBAL)
set_target_properties(convai_sdk PROPERTIES
    IMPORTED_LOCATION "${CONVAI_SDK_ROOT}/libs/ws63/libconvai_sdk.a"
    INTERFACE_INCLUDE_DIRECTORIES "${sdk_include};${sdk_include}/convai")
goldieos_add_opus(PLATFORM ws63 SOURCE_DIR "${GOLDIEOS_ROOT}")
convai_target_enable_ws63_options(opus)
goldieos_add_ws63_target(TARGET unified_ws63 PLATFORM_TARGET ws63_platform SOURCE_DIR "${GOLDIEOS_ROOT}")

# Keep the board/services libraries; exclude the old desktop, settings and AI bridge.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/common.cmake")
unified_goldieos_common(unified_ws63 ws63)
goldieos_add_ws63_mbedtls(TARGET ws63_tls SOURCE_DIR "${GOLDIEOS_ROOT}"
    SDK_INCLUDE_DIR "${sdk_include}" PLATFORM_TARGET ws63_platform)
# The BSP's libc clock is not synchronized by its NTP service. Use the same
# NTP-backed clock for certificate validity checks and SDK authentication.
target_compile_definitions(ws63_tls PRIVATE MBEDTLS_PLATFORM_TIME_ALT)
target_compile_definitions(unified_ws63 PRIVATE MBEDTLS_PLATFORM_TIME_ALT)
goldieos_add_ws63_firmware(TARGET ws63_firmware APP_TARGET unified_ws63 OPUS_TARGET opus
    SDK_TARGET convai_sdk MBEDTLS_TARGET ws63_tls DEFAULT_BOARD_ROOT "${GOLDIEOS_ROOT}"
    BASENAME goldieos VERSION "0.1.0")
# The vendor packager hard-codes goldieos.*. Publish a distinct application name.
add_custom_target(unified_ws63_image ALL
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/out/goldieos.fwpkg"
        "${CMAKE_CURRENT_BINARY_DIR}/out/unified_ai_${WS63_BOARD}.fwpkg"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_CURRENT_BINARY_DIR}/out/goldieos.bin"
        "${CMAKE_CURRENT_BINARY_DIR}/out/unified_ai_${WS63_BOARD}.bin"
    DEPENDS ws63_firmware)
