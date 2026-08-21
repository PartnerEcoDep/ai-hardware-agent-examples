# Shared ConvAI SDK target resolution and source-build policy.
#
# CONVAI_SDK_MODE selects how the SDK is obtained:
#   AUTO     - prefer CONVAI_SDK_LIBRARY, then source, then the default archive
#   SOURCE   - require SOURCE_DIR/CMakeLists.txt
#   PREBUILT - require CONVAI_SDK_LIBRARY or PREBUILT_LIBRARY
#
# CONVAI_SDK_LIBRARY is an optional machine-local archive override. It has
# precedence over PREBUILT_LIBRARY and, in AUTO mode, over a source checkout.

include_guard(GLOBAL)
include(CMakeParseArguments)

set(CONVAI_SDK_MODE "AUTO" CACHE STRING
    "ConvAI SDK provider: AUTO, SOURCE, or PREBUILT")
set_property(CACHE CONVAI_SDK_MODE PROPERTY STRINGS AUTO SOURCE PREBUILT)

set(CONVAI_SDK_LIBRARY "" CACHE FILEPATH
    "Optional ConvAI SDK prebuilt archive override")

# Apply policies that belong to a source-built SDK rather than its consumers.
# Repeated calls are safe. Imported targets are intentionally left unchanged.
function(convai_apply_sdk_policy)
    set(_one_value_args TARGET VERSION)
    cmake_parse_arguments(ARG "" "${_one_value_args}" "" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "convai_apply_sdk_policy: unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT ARG_TARGET)
        message(FATAL_ERROR "convai_apply_sdk_policy: TARGET is required")
    endif()
    if(NOT TARGET "${ARG_TARGET}")
        message(FATAL_ERROR
            "convai_apply_sdk_policy: target does not exist: ${ARG_TARGET}")
    endif()

    get_target_property(_actual_target "${ARG_TARGET}" ALIASED_TARGET)
    if(NOT _actual_target)
        set(_actual_target "${ARG_TARGET}")
    endif()

    get_target_property(_is_imported "${_actual_target}" IMPORTED)
    if(_is_imported)
        return()
    endif()

    get_target_property(_hardening_applied
        "${_actual_target}" CONVAI_SDK_HARDENING_APPLIED)
    if(NOT _hardening_applied)
        target_compile_options("${_actual_target}" PRIVATE
            -fstack-protector-strong
            -D_FORTIFY_SOURCE=2
            "$<$<CONFIG:Debug>:-ftrapv>"
        )
        set_property(TARGET "${_actual_target}" PROPERTY
            CONVAI_SDK_HARDENING_APPLIED TRUE)
    endif()

    if(NOT "${ARG_VERSION}" STREQUAL "")
        get_target_property(_applied_version
            "${_actual_target}" CONVAI_SDK_POLICY_VERSION)
        if(_applied_version AND
           NOT "${_applied_version}" STREQUAL "${ARG_VERSION}")
            message(FATAL_ERROR
                "convai_apply_sdk_policy: ${_actual_target} already uses version "
                "'${_applied_version}', cannot also apply '${ARG_VERSION}'")
        elseif(NOT _applied_version)
            target_compile_definitions("${_actual_target}" PRIVATE
                CONVAI_PROJECT_VERSION="${ARG_VERSION}"
            )
            set_property(TARGET "${_actual_target}" PROPERTY
                CONVAI_SDK_POLICY_VERSION "${ARG_VERSION}")
        endif()
    endif()
endfunction()

# Resolve a stable SDK target from source or a prebuilt archive.
#
# Required:
#   TARGET              Target name exposed to callers.
#
# Optional:
#   MODE                AUTO, SOURCE, or PREBUILT; defaults to CONVAI_SDK_MODE.
#   SOURCE_DIR          Directory whose CMakeLists.txt defines SOURCE_TARGET.
#   SOURCE_BINARY_DIR   Binary directory used for add_subdirectory().
#   SOURCE_TARGET       Target created by SOURCE_DIR; defaults to TARGET.
#   PREBUILT_LIBRARY    Platform default archive (overridden by
#                       CONVAI_SDK_LIBRARY).
#   INCLUDE_DIR         Usage include directory for an imported target.
#   VERSION             Version definition applied to a source target.
#   REQUIRED            Fail instead of returning an empty result in AUTO mode.
#   OUT_TARGET          Parent-scope variable receiving TARGET or an empty value.
#   OUT_MODE            Parent-scope variable receiving SOURCE/PREBUILT/empty.
function(convai_resolve_sdk_target)
    set(_options REQUIRED)
    set(_one_value_args
        TARGET
        MODE
        SOURCE_DIR
        SOURCE_BINARY_DIR
        SOURCE_TARGET
        PREBUILT_LIBRARY
        INCLUDE_DIR
        VERSION
        OUT_TARGET
        OUT_MODE
    )
    cmake_parse_arguments(ARG "${_options}" "${_one_value_args}" "" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "convai_resolve_sdk_target: unknown arguments: ${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT ARG_TARGET)
        message(FATAL_ERROR "convai_resolve_sdk_target: TARGET is required")
    endif()

    if(ARG_MODE)
        string(TOUPPER "${ARG_MODE}" _requested_mode)
    else()
        string(TOUPPER "${CONVAI_SDK_MODE}" _requested_mode)
    endif()
    if(NOT _requested_mode MATCHES "^(AUTO|SOURCE|PREBUILT)$")
        message(FATAL_ERROR
            "convai_resolve_sdk_target: invalid MODE '${_requested_mode}'; "
            "expected AUTO, SOURCE, or PREBUILT")
    endif()

    if(ARG_SOURCE_TARGET)
        set(_source_target "${ARG_SOURCE_TARGET}")
    else()
        set(_source_target "${ARG_TARGET}")
    endif()

    set(_source_dir "${ARG_SOURCE_DIR}")
    if(_source_dir AND NOT IS_ABSOLUTE "${_source_dir}")
        get_filename_component(_source_dir "${_source_dir}" ABSOLUTE
            BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    if(NOT "${CONVAI_SDK_LIBRARY}" STREQUAL "")
        set(_prebuilt_library "${CONVAI_SDK_LIBRARY}")
        set(_has_library_override TRUE)
    else()
        set(_prebuilt_library "${ARG_PREBUILT_LIBRARY}")
        set(_has_library_override FALSE)
    endif()
    if(_prebuilt_library AND NOT IS_ABSOLUTE "${_prebuilt_library}")
        get_filename_component(_prebuilt_library "${_prebuilt_library}" ABSOLUTE
            BASE_DIR "${CMAKE_SOURCE_DIR}")
    endif()

    set(_selected_mode "")
    set(_actual_target "")

    # An existing public target always wins; explicit modes still validate that
    # its provider is compatible with the request.
    if(TARGET "${ARG_TARGET}")
        get_target_property(_actual_target "${ARG_TARGET}" ALIASED_TARGET)
        if(NOT _actual_target)
            set(_actual_target "${ARG_TARGET}")
        endif()
        get_target_property(_existing_is_imported "${_actual_target}" IMPORTED)
        if(_existing_is_imported)
            set(_selected_mode PREBUILT)
        else()
            set(_selected_mode SOURCE)
        endif()
    elseif(TARGET "${_source_target}")
        set(_actual_target "${_source_target}")
        get_target_property(_existing_is_imported "${_actual_target}" IMPORTED)
        if(_existing_is_imported)
            set(_selected_mode PREBUILT)
        else()
            set(_selected_mode SOURCE)
        endif()
    endif()

    if(_selected_mode)
        if(_requested_mode STREQUAL "SOURCE" AND
           NOT _selected_mode STREQUAL "SOURCE")
            message(FATAL_ERROR
                "convai_resolve_sdk_target: SOURCE requested, but '${ARG_TARGET}' "
                "is already an imported target")
        elseif(_requested_mode STREQUAL "PREBUILT" AND
               NOT _selected_mode STREQUAL "PREBUILT")
            message(FATAL_ERROR
                "convai_resolve_sdk_target: PREBUILT requested, but '${ARG_TARGET}' "
                "is already source-built")
        endif()
    elseif(_requested_mode STREQUAL "SOURCE")
        set(_selected_mode SOURCE)
    elseif(_requested_mode STREQUAL "PREBUILT")
        set(_selected_mode PREBUILT)
    elseif(_has_library_override)
        set(_selected_mode PREBUILT)
    elseif(_source_dir AND EXISTS "${_source_dir}/CMakeLists.txt")
        set(_selected_mode SOURCE)
    elseif(_prebuilt_library AND EXISTS "${_prebuilt_library}")
        set(_selected_mode PREBUILT)
    endif()

    if(NOT _selected_mode)
        if(ARG_REQUIRED)
            message(FATAL_ERROR
                "ConvAI SDK not found. Provide SOURCE_DIR containing CMakeLists.txt, "
                "or set CONVAI_SDK_LIBRARY/PREBUILT_LIBRARY to an existing archive.")
        endif()
        if(ARG_OUT_TARGET)
            set("${ARG_OUT_TARGET}" "" PARENT_SCOPE)
        endif()
        if(ARG_OUT_MODE)
            set("${ARG_OUT_MODE}" "" PARENT_SCOPE)
        endif()
        return()
    endif()

    if(_selected_mode STREQUAL "SOURCE")
        if(NOT _actual_target)
            if(NOT _source_dir OR NOT EXISTS "${_source_dir}/CMakeLists.txt")
                message(FATAL_ERROR
                    "convai_resolve_sdk_target: SDK source CMakeLists.txt not found: "
                    "${_source_dir}/CMakeLists.txt")
            endif()

            if(ARG_SOURCE_BINARY_DIR)
                set(_source_binary_dir "${ARG_SOURCE_BINARY_DIR}")
                if(NOT IS_ABSOLUTE "${_source_binary_dir}")
                    get_filename_component(_source_binary_dir
                        "${_source_binary_dir}" ABSOLUTE
                        BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
                endif()
            else()
                set(_source_binary_dir
                    "${CMAKE_CURRENT_BINARY_DIR}/convai-sdk-source")
            endif()

            add_subdirectory("${_source_dir}" "${_source_binary_dir}")
            if(NOT TARGET "${_source_target}")
                message(FATAL_ERROR
                    "ConvAI SDK source build did not define target "
                    "'${_source_target}': ${_source_dir}")
            endif()
            set(_actual_target "${_source_target}")
        endif()

        if(NOT TARGET "${ARG_TARGET}")
            add_library("${ARG_TARGET}" ALIAS "${_actual_target}")
        endif()
        convai_apply_sdk_policy(
            TARGET "${_actual_target}"
            VERSION "${ARG_VERSION}"
        )
        message(STATUS
            "ConvAI SDK: building '${ARG_TARGET}' from source: ${_source_dir}")
    else()
        if(NOT _actual_target)
            if(NOT _prebuilt_library OR NOT EXISTS "${_prebuilt_library}")
                message(FATAL_ERROR
                    "convai_resolve_sdk_target: prebuilt SDK archive not found: "
                    "${_prebuilt_library}")
            endif()

            add_library("${ARG_TARGET}" STATIC IMPORTED GLOBAL)
            set_target_properties("${ARG_TARGET}" PROPERTIES
                IMPORTED_LOCATION "${_prebuilt_library}"
            )
            if(ARG_INCLUDE_DIR)
                set_property(TARGET "${ARG_TARGET}" PROPERTY
                    INTERFACE_INCLUDE_DIRECTORIES "${ARG_INCLUDE_DIR}")
            endif()
            set(_actual_target "${ARG_TARGET}")
        endif()
        if(NOT TARGET "${ARG_TARGET}")
            add_library("${ARG_TARGET}" ALIAS "${_actual_target}")
        endif()
        message(STATUS
            "ConvAI SDK: using prebuilt '${ARG_TARGET}': ${_prebuilt_library}")
    endif()

    set_property(TARGET "${_actual_target}" PROPERTY
        CONVAI_SDK_RESOLUTION_MODE "${_selected_mode}")

    if(ARG_OUT_TARGET)
        set("${ARG_OUT_TARGET}" "${ARG_TARGET}" PARENT_SCOPE)
    endif()
    if(ARG_OUT_MODE)
        set("${ARG_OUT_MODE}" "${_selected_mode}" PARENT_SCOPE)
    endif()
endfunction()
