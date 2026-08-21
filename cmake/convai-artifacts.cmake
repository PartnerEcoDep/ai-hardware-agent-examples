# Helpers for producing version-qualified copies of build artifacts.

include_guard(GLOBAL)
include(CMakeParseArguments)

# Copy an executable or library after it is built, preserving its target file
# base name and suffix:
#   <target-file-dir>/<target-base-name>-<version><target-suffix>
function(convai_add_versioned_target_copy)
    set(_one_value_args TARGET VERSION OUTPUT_DIR)
    cmake_parse_arguments(ARG "" "${_one_value_args}" "" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "convai_add_versioned_target_copy: unknown arguments: "
            "${ARG_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT ARG_TARGET)
        message(FATAL_ERROR
            "convai_add_versioned_target_copy: TARGET is required")
    endif()
    if(NOT TARGET "${ARG_TARGET}")
        message(FATAL_ERROR
            "convai_add_versioned_target_copy: target does not exist: "
            "${ARG_TARGET}")
    endif()
    if("${ARG_VERSION}" STREQUAL "")
        message(FATAL_ERROR
            "convai_add_versioned_target_copy: VERSION is required")
    endif()

    if(ARG_OUTPUT_DIR)
        set(_output_dir "${ARG_OUTPUT_DIR}")
    else()
        set(_output_dir "$<TARGET_FILE_DIR:${ARG_TARGET}>")
    endif()

    set(_versioned_file
        "${_output_dir}/$<TARGET_FILE_BASE_NAME:${ARG_TARGET}>-${ARG_VERSION}$<TARGET_FILE_SUFFIX:${ARG_TARGET}>")

    add_custom_command(TARGET "${ARG_TARGET}" POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_output_dir}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${ARG_TARGET}>"
            "${_versioned_file}"
        # Keep COMMENT free of generator expressions for CMake 3.21.
        COMMENT "Versioned artifact copy for ${ARG_TARGET}: ${ARG_VERSION}"
        VERBATIM
    )
endfunction()

# Add versioned copies for a set of existing files after TARGET is built.
# For each extension, this copies:
#   <output>/<basename>.<ext> -> <output>/<basename>-<version>.<ext>
function(convai_add_versioned_files)
    set(_one_value_args TARGET OUTPUT_DIR BASENAME VERSION)
    set(_multi_value_args EXTENSIONS)
    cmake_parse_arguments(ARG "" "${_one_value_args}"
        "${_multi_value_args}" ${ARGN})

    if(ARG_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "convai_add_versioned_files: unknown arguments: "
            "${ARG_UNPARSED_ARGUMENTS}")
    endif()
    foreach(_required_arg IN ITEMS TARGET OUTPUT_DIR BASENAME VERSION)
        if("${ARG_${_required_arg}}" STREQUAL "")
            message(FATAL_ERROR
                "convai_add_versioned_files: ${_required_arg} is required")
        endif()
    endforeach()
    if(NOT TARGET "${ARG_TARGET}")
        message(FATAL_ERROR
            "convai_add_versioned_files: target does not exist: ${ARG_TARGET}")
    endif()
    if(NOT ARG_EXTENSIONS)
        message(FATAL_ERROR
            "convai_add_versioned_files: EXTENSIONS is required")
    endif()

    foreach(_extension IN LISTS ARG_EXTENSIONS)
        string(REGEX REPLACE "^\\." "" _extension "${_extension}")
        if("${_extension}" STREQUAL "")
            message(FATAL_ERROR
                "convai_add_versioned_files: extensions must not be empty")
        endif()

        set(_source_file
            "${ARG_OUTPUT_DIR}/${ARG_BASENAME}.${_extension}")
        set(_versioned_file
            "${ARG_OUTPUT_DIR}/${ARG_BASENAME}-${ARG_VERSION}.${_extension}")

        add_custom_command(TARGET "${ARG_TARGET}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${_source_file}"
                "${_versioned_file}"
            COMMENT
                "Versioned artifact copy: ${ARG_BASENAME}-${ARG_VERSION}.${_extension}"
            VERBATIM
        )
    endforeach()
endfunction()
