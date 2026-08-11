# ESP-IDF component wrapper for the ConvAI SDK.
#
# Selection matches the GoldieOS build:
#   1. Build ../../../../src when its CMakeLists.txt is present.
#   2. Otherwise use the ESP32-S3 prebuilt archive.
#
# A source package must define a target named `convai_sdk`, which is the same
# contract used by the repository's top-level GoldieOS CMake build.

get_filename_component(CONVAI_REPO_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)

set(CONVAI_SDK_SOURCE_DIR "${CONVAI_REPO_ROOT}/src")
set(CONVAI_ESP32S3_ARCHIVE
    "${CONVAI_REPO_ROOT}/libs/esp32-s3/libconvai_sdk.a")

idf_component_register(
    INCLUDE_DIRS "${CONVAI_REPO_ROOT}/include"
)

if(EXISTS "${CONVAI_SDK_SOURCE_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${CONVAI_SDK_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/convai_sdk_source"
    )

    if(NOT TARGET convai_sdk)
        message(FATAL_ERROR
            "ConvAI source build did not define the required 'convai_sdk' target: "
            "${CONVAI_SDK_SOURCE_DIR}")
    endif()

    target_link_libraries(${COMPONENT_LIB} INTERFACE convai_sdk)
    message(STATUS "ESP32-S3: building ConvAI SDK from source: ${CONVAI_SDK_SOURCE_DIR}")
elseif(EXISTS "${CONVAI_ESP32S3_ARCHIVE}")
    add_prebuilt_library(convai_sdk_prebuilt "${CONVAI_ESP32S3_ARCHIVE}")
    target_link_libraries(${COMPONENT_LIB} INTERFACE convai_sdk_prebuilt)
    message(STATUS "ESP32-S3: using prebuilt ConvAI SDK: ${CONVAI_ESP32S3_ARCHIVE}")
else()
    message(FATAL_ERROR
        "ConvAI SDK not found. Provide either source at "
        "${CONVAI_SDK_SOURCE_DIR}/CMakeLists.txt or the ESP32-S3 archive at "
        "${CONVAI_ESP32S3_ARCHIVE}")
endif()
