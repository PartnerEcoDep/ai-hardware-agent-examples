include_guard(GLOBAL)

include(CMakeParseArguments)

function(goldieos_add_win_target)
    cmake_parse_arguments(PARSE_ARGV 0 _arg "" "TARGET;SOURCE_DIR;VERSION" "")

    if(NOT _arg_TARGET)
        message(FATAL_ERROR "goldieos_add_win_target requires TARGET")
    endif()
    if(TARGET "${_arg_TARGET}")
        message(FATAL_ERROR
            "goldieos_add_win_target target already exists: ${_arg_TARGET}")
    endif()
    if(NOT _arg_SOURCE_DIR)
        message(FATAL_ERROR "goldieos_add_win_target requires SOURCE_DIR")
    endif()
    if(NOT _arg_VERSION)
        message(FATAL_ERROR "goldieos_add_win_target requires VERSION")
    endif()
    if(NOT TARGET convai_sdk)
        message(FATAL_ERROR
            "goldieos_add_win_target requires the convai_sdk target")
    endif()
    if(NOT COMMAND convai_add_versioned_target_copy)
        message(FATAL_ERROR
            "goldieos_add_win_target requires convai_add_versioned_target_copy")
    endif()

    file(GLOB _win_cjson_sources CONFIGURE_DEPENDS
        "${_arg_SOURCE_DIR}/third_party/cjson/*.c"
    )
    set(_win_platform_sources
        "${_arg_SOURCE_DIR}/platform/convai_platform_win.c"
        "${_arg_SOURCE_DIR}/platform/win/goldie_osal_win_compat.c"
    )

    add_executable("${_arg_TARGET}")
    goldieos_target_add_common(
        TARGET "${_arg_TARGET}"
        SOURCE_DIR "${_arg_SOURCE_DIR}"
        AFTER_INIT_SOURCES ${_win_platform_sources}
        FINAL_SOURCES ${_win_cjson_sources}
        PLATFORM_INCLUDE_DIRS
            "${_arg_SOURCE_DIR}/include/platform/win10"
    )

    # Hardware-only feature macros intentionally remain disabled for the
    # simulator so those implementations compile to their no-op variants.
    target_compile_definitions("${_arg_TARGET}" PRIVATE
        PLATFORM_TYPE_WIN
        CONFIG_APP_ENABLE_OPUS=1
        ST7789_SPI_LCD
        TF_LITE_STATIC_MEMORY
        TF_LITE_DISABLE_X86_NEON
        TF_LITE_STRIP_ERROR_STRINGS
    )
    convai_target_enable_warnings("${_arg_TARGET}")

    # Windows simulator linker policy. The fixed low image base keeps pointers
    # below 2 GiB so the prebuilt SDK's cltq-based conversion remains valid.
    target_link_options("${_arg_TARGET}" PRIVATE
        -mconsole
        -static-libgcc
        -static-libstdc++
        -static
        -Wl,--gc-sections
        -Wl,--undefined=__start_goldie_init_table
        -Wl,--undefined=__stop_goldie_init_table
        -Wl,--disable-dynamicbase
        -Wl,--image-base,0x10000000
    )

    set(_win10_lib_dir "${_arg_SOURCE_DIR}/libs/win10")
    set(_win_link_libraries
        -Wl,--whole-archive
        "${_win10_lib_dir}/libwinvm.a"
        "${_win10_lib_dir}/libtiny_core.a"
        "${_win10_lib_dir}/libtiny_gui.a"
        -Wl,--no-whole-archive
        convai_sdk
        "${_win10_lib_dir}/libmbedtls.a"
        "${_win10_lib_dir}/libmbedx509.a"
        "${_win10_lib_dir}/libmbedcrypto.a"
        "${_win10_lib_dir}/libaud_algo.a"
        "${_win10_lib_dir}/libtflm_lib.a"
        winmm
        ws2_32
        gdi32
        user32
    )
    if(TARGET opus)
        list(APPEND _win_link_libraries opus)
    endif()
    target_link_libraries("${_arg_TARGET}" PRIVATE ${_win_link_libraries})

    set(_runtime_config
        "${_arg_SOURCE_DIR}/sdk_integration/convai.cfg.example")
    add_custom_command(TARGET "${_arg_TARGET}" POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${_runtime_config}"
            "$<TARGET_FILE_DIR:${_arg_TARGET}>/convai.cfg.example"
        COMMAND if not exist \"$<TARGET_FILE_DIR:${_arg_TARGET}>/convai.cfg\"
            "${CMAKE_COMMAND}" -E copy
            "${_runtime_config}"
            \"$<TARGET_FILE_DIR:${_arg_TARGET}>/convai.cfg\"
        COMMENT "Copying runtime config template (convai.cfg)"
    )

    convai_add_versioned_target_copy(
        TARGET "${_arg_TARGET}"
        VERSION "${_arg_VERSION}"
    )

    message(STATUS
        "GoldieOS WIN simulator configured. Compiler: ${CMAKE_C_COMPILER}")
endfunction()
