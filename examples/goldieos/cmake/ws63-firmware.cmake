include_guard(GLOBAL)

include(CMakeParseArguments)

# Package a WS63 application with the donor board libraries and build tools.
# All mutable inputs are copied to a configuration-local staging directory so
# concurrent presets never modify or race on the source board package.
function(goldieos_add_ws63_firmware)
    set(_options)
    set(_one_value_args
        TARGET
        APP_TARGET
        OPUS_TARGET
        SDK_TARGET
        MBEDTLS_TARGET
        DEFAULT_BOARD_ROOT
        BASENAME
        VERSION
    )
    cmake_parse_arguments(WS63_FIRMWARE
        "${_options}" "${_one_value_args}" "" ${ARGN})

    foreach(_required_arg IN ITEMS
            TARGET APP_TARGET OPUS_TARGET SDK_TARGET MBEDTLS_TARGET
            DEFAULT_BOARD_ROOT BASENAME VERSION)
        if(NOT WS63_FIRMWARE_${_required_arg})
            message(FATAL_ERROR
                "goldieos_add_ws63_firmware requires ${_required_arg}")
        endif()
    endforeach()
    if(WS63_FIRMWARE_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "goldieos_add_ws63_firmware received unknown arguments: "
            "${WS63_FIRMWARE_UNPARSED_ARGUMENTS}")
    endif()
    if(TARGET "${WS63_FIRMWARE_TARGET}")
        message(FATAL_ERROR
            "Target already exists: ${WS63_FIRMWARE_TARGET}")
    endif()
    foreach(_dependency_target IN ITEMS
            APP_TARGET OPUS_TARGET SDK_TARGET MBEDTLS_TARGET)
        if(NOT TARGET "${WS63_FIRMWARE_${_dependency_target}}")
            message(FATAL_ERROR
                "WS63 firmware dependency target does not exist: "
                "${WS63_FIRMWARE_${_dependency_target}}")
        endif()
    endforeach()
    if(NOT COMMAND convai_add_versioned_files)
        message(FATAL_ERROR
            "goldieos_add_ws63_firmware requires "
            "convai_add_versioned_files")
    endif()

    # Preserve the public override order used by the original build:
    # explicit cache/command-line value, environment, then repository default.
    if(WS63_BOARD_ROOT)
        set(_link_root "${WS63_BOARD_ROOT}")
    elseif(DEFINED ENV{GOLDIEOS_ROOT})
        set(_link_root "$ENV{GOLDIEOS_ROOT}")
    else()
        set(_link_root "${WS63_FIRMWARE_DEFAULT_BOARD_ROOT}")
    endif()

    if(NOT EXISTS "${_link_root}")
        message(FATAL_ERROR
            "WS63 board package not found at: ${_link_root}\n"
            "Set WS63_BOARD_ROOT cmake variable or GOLDIEOS_ROOT "
            "environment variable.")
    endif()

    set(_board_mbedtls "${_link_root}/libs/ws63/board/libmbedtls.a")
    if(NOT EXISTS "${_board_mbedtls}")
        message(FATAL_ERROR
            "Board mbedTLS archive not found: ${_board_mbedtls}")
    endif()

    set(_link_tools_dir "${_link_root}/tools/build/tools")
    set(_linker "${_link_tools_dir}/ws63_link_v4.exe")
    set(_sign_tool "${_link_tools_dir}/ws63_sign_tool.exe")
    if(NOT EXISTS "${_linker}")
        message(FATAL_ERROR "WS63 linker not found: ${_linker}")
    endif()
    if(NOT EXISTS "${_sign_tool}")
        message(FATAL_ERROR "WS63 signing tool not found: ${_sign_tool}")
    endif()

    set(_output_dir "${CMAKE_CURRENT_BINARY_DIR}/out")
    set(_output_bin
        "${_output_dir}/${WS63_FIRMWARE_BASENAME}.bin")
    set(_output_elf
        "${_output_dir}/${WS63_FIRMWARE_BASENAME}.elf")
    set(_output_fwpkg
        "${_output_dir}/${WS63_FIRMWARE_BASENAME}.fwpkg")

    set(_stage_root "${CMAKE_CURRENT_BINARY_DIR}/ws63-link-root")
    set(_stage_lib_root "${_stage_root}/libs/ws63")
    set(_stage_config_dir "${_stage_root}/tools/build/config/ws63")
    set(_stage_mbedtls "${_stage_lib_root}/board/libmbedtls.a")
    get_filename_component(_compiler_bin "${CMAKE_CXX_COMPILER}" DIRECTORY)

    # Every staged file is a dependency, including files added after the first
    # configure. This keeps incremental firmware packages in sync with updates
    # to either the board archives or the linker configuration.
    file(GLOB_RECURSE _link_package_inputs
        CONFIGURE_DEPENDS
        LIST_DIRECTORIES false
        "${_link_root}/libs/ws63/*"
        "${_link_root}/tools/build/config/ws63/*"
    )

    add_custom_command(
        OUTPUT "${_output_fwpkg}"
        BYPRODUCTS "${_output_bin}" "${_output_elf}"

        COMMAND "${CMAKE_COMMAND}" -E remove_directory "${_stage_root}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_output_dir}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_stage_lib_root}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${_link_root}/libs/ws63"
            "${_stage_lib_root}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_stage_config_dir}"
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${_link_root}/tools/build/config/ws63"
            "${_stage_config_dir}"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:${WS63_FIRMWARE_MBEDTLS_TARGET}>"
            "${_stage_mbedtls}"

        # ws63_link_v4 launches the compiler and signer by bare executable
        # name, so expose the selected toolchain and the board tools explicitly.
        COMMAND "${CMAKE_COMMAND}" -E env
            "PATH=${_compiler_bin};${_link_tools_dir};$ENV{PATH}"
            "${_linker}"
            "${_stage_root}"
            "$<TARGET_FILE:${WS63_FIRMWARE_APP_TARGET}>,$<TARGET_FILE:${WS63_FIRMWARE_OPUS_TARGET}>,$<TARGET_FILE:${WS63_FIRMWARE_SDK_TARGET}>"
            "${_output_dir}"

        DEPENDS
            "${WS63_FIRMWARE_APP_TARGET}"
            "${WS63_FIRMWARE_OPUS_TARGET}"
            "${WS63_FIRMWARE_SDK_TARGET}"
            "${WS63_FIRMWARE_MBEDTLS_TARGET}"
            ${_link_package_inputs}
            "${_linker}"
            "${_sign_tool}"

        COMMENT "Generating ${WS63_FIRMWARE_BASENAME} firmware with rebuilt WS63 mbedTLS"
        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        VERBATIM
    )

    add_custom_target("${WS63_FIRMWARE_TARGET}" ALL
        DEPENDS "${_output_fwpkg}"
        COMMENT "Firmware: ${_output_fwpkg}"
    )

    convai_add_versioned_files(
        TARGET "${WS63_FIRMWARE_TARGET}"
        OUTPUT_DIR "${_output_dir}"
        BASENAME "${WS63_FIRMWARE_BASENAME}"
        VERSION "${WS63_FIRMWARE_VERSION}"
        EXTENSIONS fwpkg bin
    )
endfunction()
