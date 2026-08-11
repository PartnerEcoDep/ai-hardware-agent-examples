# ESP-IDF component wrapper for the ConvAI SDK.
#
# Selection matches the GoldieOS build:
#   1. Build ../../../../src when its CMakeLists.txt is present, and export the
#      resulting ESP32-S3 archive below the IDF build directory.
#   2. Otherwise use the ESP32-S3 prebuilt archive to build the firmware.
#
# A source package must define a target named `convai_sdk`, which is the same
# contract used by the repository's top-level GoldieOS CMake build.

get_filename_component(CONVAI_REPO_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)

set(CONVAI_SDK_SOURCE_DIR "${CONVAI_REPO_ROOT}/src")
set(CONVAI_ESP32S3_ARCHIVE
    "${CONVAI_REPO_ROOT}/libs/esp32-s3/libconvai_sdk.a")
set(CONVAI_ESP32S3_ARTIFACT_DIR
    "${CMAKE_BINARY_DIR}/artifacts/esp32-s3")
set(CONVAI_ESP32S3_ARTIFACT
    "${CONVAI_ESP32S3_ARTIFACT_DIR}/libconvai_sdk.a")

if(NOT IDF_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR
        "The ConvAI ESP32-S3 component requires IDF_TARGET=esp32s3; got "
        "'${IDF_TARGET}'")
endif()

idf_component_register(
    INCLUDE_DIRS "${CONVAI_REPO_ROOT}/include"
)

# Track the exact source manifest so adding or removing it causes CMake to
# reconfigure an existing IDF build directory.
file(GLOB CONVAI_SDK_SOURCE_MANIFEST CONFIGURE_DEPENDS
    "${CONVAI_SDK_SOURCE_DIR}/CMakeLists.txt")

if(CONVAI_SDK_SOURCE_MANIFEST)
    if(TARGET convai_sdk)
        message(FATAL_ERROR
            "Unexpected pre-existing CMake target 'convai_sdk'")
    endif()

    # Source packages can use either the IDF variables ESP_PLATFORM/IDF_TARGET
    # or this platform value. Keep it directory-scoped instead of modifying the
    # global CMake cache.
    set(CONVAI_PLATFORM "esp32s3")

    # ESP-IDF evaluates component CMake files in an isolated scope. Require the
    # shared version explicitly before forwarding it to src/.
    if(NOT DEFINED CONVAI_PROJECT_VERSION OR
       CONVAI_PROJECT_VERSION STREQUAL "")
        message(FATAL_ERROR
            "CONVAI_PROJECT_VERSION is not defined; include the shared "
            "cmake/ConvaiVersion.cmake before the ESP-IDF project() call")
    endif()
    add_subdirectory(
        "${CONVAI_SDK_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/convai_sdk_source"
    )

    if(NOT TARGET convai_sdk)
        message(FATAL_ERROR
            "ConvAI source build did not define the required 'convai_sdk' target: "
            "${CONVAI_SDK_SOURCE_DIR}")
    endif()

    # Resolve a possible alias before checking and archiving the source target.
    get_target_property(CONVAI_SDK_REAL_TARGET convai_sdk ALIASED_TARGET)
    if(NOT CONVAI_SDK_REAL_TARGET)
        set(CONVAI_SDK_REAL_TARGET convai_sdk)
    endif()

    get_target_property(CONVAI_SDK_TARGET_TYPE
        "${CONVAI_SDK_REAL_TARGET}" TYPE)
    get_target_property(CONVAI_SDK_TARGET_IMPORTED
        "${CONVAI_SDK_REAL_TARGET}" IMPORTED)
    if(CONVAI_SDK_TARGET_IMPORTED OR
       NOT CONVAI_SDK_TARGET_TYPE STREQUAL "STATIC_LIBRARY")
        message(FATAL_ERROR
            "ConvAI source target 'convai_sdk' must be a local STATIC_LIBRARY; "
            "got type '${CONVAI_SDK_TARGET_TYPE}'")
    endif()

    # CMAKE_SOURCE_DIR is the ESP-IDF example when src is embedded here. Add
    # the repository API path explicitly; the source target remains responsible
    # for declaring any idf:: component dependencies it directly uses.
    target_include_directories("${CONVAI_SDK_REAL_TARGET}" PUBLIC
        "${CONVAI_REPO_ROOT}/include")

    # CMake variables are not visible to C/C++ sources by themselves. Export
    # the resolved version only for a source-built SDK target; the prebuilt
    # archive already contains the version it was compiled with.
    target_compile_definitions("${CONVAI_SDK_REAL_TARGET}" PRIVATE
        CONVAI_PROJECT_VERSION="${CONVAI_PROJECT_VERSION}"
    )

    target_link_libraries(${COMPONENT_LIB} INTERFACE convai_sdk)

    # The firmware and exported SDK archive use the same target, toolchain,
    # compile definitions, and object files. Do not overwrite the checked-in
    # fallback archive automatically.
    add_custom_command(
        OUTPUT "${CONVAI_ESP32S3_ARTIFACT}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory
            "${CONVAI_ESP32S3_ARTIFACT_DIR}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${CONVAI_SDK_REAL_TARGET}>"
            "${CONVAI_ESP32S3_ARTIFACT}"
        DEPENDS "${CONVAI_SDK_REAL_TARGET}"
        COMMENT "Archiving ESP32-S3 ConvAI SDK"
        VERBATIM
    )
    add_custom_target(convai_sdk_esp32s3_archive ALL
        DEPENDS "${CONVAI_ESP32S3_ARTIFACT}")

    message(STATUS
        "ESP32-S3 ConvAI SDK provider: source (${CONVAI_SDK_SOURCE_DIR})")
    message(STATUS
        "ESP32-S3 ConvAI SDK archive: ${CONVAI_ESP32S3_ARTIFACT}")
elseif(EXISTS "${CONVAI_ESP32S3_ARCHIVE}")
    add_prebuilt_library(convai_sdk_prebuilt "${CONVAI_ESP32S3_ARCHIVE}")
    target_link_libraries(${COMPONENT_LIB} INTERFACE convai_sdk_prebuilt)
    message(STATUS
        "ESP32-S3 ConvAI SDK provider: prebuilt (${CONVAI_ESP32S3_ARCHIVE})")
else()
    message(FATAL_ERROR
        "ConvAI SDK not found. Provide either source at "
        "${CONVAI_SDK_SOURCE_DIR}/CMakeLists.txt or the ESP32-S3 archive at "
        "${CONVAI_ESP32S3_ARCHIVE}")
endif()
