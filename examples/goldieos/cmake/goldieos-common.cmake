include_guard(GLOBAL)

include(CMakeParseArguments)

# Configure and add the bundled Opus target. The caller deliberately owns the
# decision to link opus so platform helpers can preserve their link ordering.
function(goldieos_add_opus)
    cmake_parse_arguments(PARSE_ARGV 0 _arg "" "PLATFORM;SOURCE_DIR" "")

    if(NOT _arg_PLATFORM)
        message(FATAL_ERROR "goldieos_add_opus requires PLATFORM")
    endif()
    if(NOT _arg_SOURCE_DIR)
        message(FATAL_ERROR "goldieos_add_opus requires SOURCE_DIR")
    endif()

    set(_opus_dir "${_arg_SOURCE_DIR}/third_party/opus-1.6.1")
    if(NOT EXISTS "${_opus_dir}/CMakeLists.txt")
        return()
    endif()
    if(TARGET opus)
        return()
    endif()

    set(OPUS_VAR_ARRAYS OFF CACHE BOOL "Disable variable length arrays" FORCE)
    set(OPUS_USE_ALLOCA OFF CACHE BOOL "Disable alloca" FORCE)
    set(OPUS_FIXED_POINT ON CACHE BOOL "Enable fixed-point" FORCE)
    set(OPUS_ASM OFF CACHE BOOL "Disable assembly" FORCE)
    set(OPUS_INTRINSICS OFF CACHE BOOL "Disable intrinsics" FORCE)
    set(OPUS_DISABLE_FLOAT_API ON CACHE BOOL "Disable float API" FORCE)
    set(OPUS_ENABLE_FLOAT_API OFF CACHE BOOL "Enable float API" FORCE)
    set(OPUS_CUSTOM_MODES ON CACHE BOOL "Enable custom modes" FORCE)

    if(_arg_PLATFORM STREQUAL "ws63")
        set(OPUS_DRED OFF CACHE BOOL "Disable DRED" FORCE)
        set(OPUS_OSCE OFF CACHE BOOL "Disable OSCE" FORCE)
        set(OPUS_FORTIFY_SOURCE OFF CACHE BOOL "Disable fortify" FORCE)
    elseif(_arg_PLATFORM STREQUAL "goldieos"
            OR _arg_PLATFORM STREQUAL "goldieos-win"
            OR _arg_PLATFORM STREQUAL "win")
        set(OPUS_NONTHREADSAFE_PSEUDOSTACK ON CACHE BOOL
            "Enable pseudostack" FORCE)
        set(OPUS_CUSTOM_API ON CACHE BOOL "Enable custom API" FORCE)
        set(OPUS_X86_MAY_HAVE_SSE OFF CACHE BOOL "Disable SSE" FORCE)
        set(OPUS_X86_MAY_HAVE_SSE2 OFF CACHE BOOL "Disable SSE2" FORCE)
        set(OPUS_X86_MAY_HAVE_SSE4_1 OFF CACHE BOOL "Disable SSE4.1" FORCE)
        set(OPUS_X86_MAY_HAVE_AVX2 OFF CACHE BOOL "Disable AVX2" FORCE)
        set(OPUS_X86_PRESUME_SSE OFF CACHE BOOL "Disable SSE presume" FORCE)
        set(OPUS_X86_PRESUME_SSE2 OFF CACHE BOOL "Disable SSE2 presume" FORCE)
        set(OPUS_X86_PRESUME_SSE4_1 OFF CACHE BOOL
            "Disable SSE4.1 presume" FORCE)
        set(OPUS_X86_PRESUME_AVX2 OFF CACHE BOOL "Disable AVX2 presume" FORCE)
    else()
        message(FATAL_ERROR
            "goldieos_add_opus received unsupported PLATFORM: ${_arg_PLATFORM}")
    endif()

    add_subdirectory("${_opus_dir}" "${CMAKE_CURRENT_BINARY_DIR}/opus_build")
endfunction()

# Attach all source files, include directories and compile definitions shared by
# the WS63 firmware and Windows simulator to an existing target.
function(goldieos_target_add_common)
    cmake_parse_arguments(PARSE_ARGV 0 _arg "" "TARGET;SOURCE_DIR"
        "AFTER_INIT_SOURCES;AFTER_SERVICE_SOURCES;FINAL_SOURCES;PLATFORM_INCLUDE_DIRS")

    if(NOT _arg_TARGET)
        message(FATAL_ERROR "goldieos_target_add_common requires TARGET")
    endif()
    if(NOT TARGET "${_arg_TARGET}")
        message(FATAL_ERROR
            "goldieos_target_add_common target does not exist: ${_arg_TARGET}")
    endif()
    if(NOT _arg_SOURCE_DIR)
        message(FATAL_ERROR "goldieos_target_add_common requires SOURCE_DIR")
    endif()

    file(GLOB _service_sources CONFIGURE_DEPENDS
        "${_arg_SOURCE_DIR}/services/alarm_service/*.c"
        "${_arg_SOURCE_DIR}/services/ntp_service/*.c"
        "${_arg_SOURCE_DIR}/services/aud_algo/*.c"
    )
    file(GLOB _sdk_integration_sources CONFIGURE_DEPENDS
        "${_arg_SOURCE_DIR}/sdk_integration/*.c"
    )
    file(GLOB _app_sources CONFIGURE_DEPENDS
        "${_arg_SOURCE_DIR}/apps/launcher/*.cpp"
        "${_arg_SOURCE_DIR}/apps/settings/*.cpp"
        "${_arg_SOURCE_DIR}/apps/settings/*.c"
        "${_arg_SOURCE_DIR}/apps/alarm/*.cpp"
        "${_arg_SOURCE_DIR}/apps/animaton_player/*.cpp"
        "${_arg_SOURCE_DIR}/apps/shut_down/*.cpp"
        "${_arg_SOURCE_DIR}/apps/charging_only/*.cpp"
    )
    file(GLOB _third_party_sources CONFIGURE_DEPENDS
        "${_arg_SOURCE_DIR}/third_party/fatfs-R0.11/src/*.c"
        "${_arg_SOURCE_DIR}/third_party/fatfs-R0.11/src/option/*.c"
        "${_arg_SOURCE_DIR}/third_party/webrtc_vad/*.c"
        "${_arg_SOURCE_DIR}/third_party/webrtc_vad/signal_processing/*.c"
        "${_arg_SOURCE_DIR}/third_party/webrtc_vad/vad/*.c"
        "${_arg_SOURCE_DIR}/third_party/helix/*.c"
        "${_arg_SOURCE_DIR}/third_party/helix/real/*.c"
    )

    target_sources("${_arg_TARGET}" PRIVATE
        "${_arg_SOURCE_DIR}/init/system_init.c"
        ${_arg_AFTER_INIT_SOURCES}
        ${_service_sources}
        ${_arg_AFTER_SERVICE_SOURCES}
        ${_sdk_integration_sources}
        ${_app_sources}
        ${_third_party_sources}
        ${_arg_FINAL_SOURCES}
    )

    target_include_directories("${_arg_TARGET}" PRIVATE
        "${CMAKE_SOURCE_DIR}/include"
        "${_arg_SOURCE_DIR}/sdk_integration"
        "${_arg_SOURCE_DIR}/compat"
        "${_arg_SOURCE_DIR}/include"
        "${_arg_SOURCE_DIR}/include/core"
        "${_arg_SOURCE_DIR}/include/osal"
        "${_arg_SOURCE_DIR}/include/services"
        "${_arg_SOURCE_DIR}/include/services/alarm"
        "${_arg_SOURCE_DIR}/include/services/ntp"
        "${_arg_SOURCE_DIR}/include/services/audio"
        "${_arg_SOURCE_DIR}/include/system_res"
        "${_arg_SOURCE_DIR}/include/third_party"
        "${_arg_SOURCE_DIR}/include/third_party/cjson"
        "${_arg_SOURCE_DIR}/include/third_party/mbedtls"
        "${_arg_SOURCE_DIR}/third_party"
        "${_arg_SOURCE_DIR}/third_party/fatfs-R0.11/src"
        "${_arg_SOURCE_DIR}/third_party/webrtc_vad"
        "${_arg_SOURCE_DIR}/third_party/helix/real"
        "${_arg_SOURCE_DIR}/third_party/helix/pub"
        "${_arg_SOURCE_DIR}/third_party/opus-1.6.1/include"
        "${_arg_SOURCE_DIR}/include/gui"
        ${_arg_PLATFORM_INCLUDE_DIRS}
        "${_arg_SOURCE_DIR}/apps/launcher"
        "${_arg_SOURCE_DIR}/apps/launcher/assets"
        "${_arg_SOURCE_DIR}/apps/animaton_player"
        "${_arg_SOURCE_DIR}/apps/animaton_player/assets"
        "${_arg_SOURCE_DIR}/apps/recorder"
        "${_arg_SOURCE_DIR}/apps/recorder/assets"
        "${_arg_SOURCE_DIR}/apps/AItalk"
        "${_arg_SOURCE_DIR}/apps/AItalk/assets"
        "${_arg_SOURCE_DIR}/apps/settings"
        "${_arg_SOURCE_DIR}/apps/settings/assets"
        "${_arg_SOURCE_DIR}/apps/alarm"
        "${_arg_SOURCE_DIR}/apps/alarm/assets"
        "${_arg_SOURCE_DIR}/apps/shut_down"
        "${_arg_SOURCE_DIR}/apps/shut_down/assets"
        "${_arg_SOURCE_DIR}/apps/charging_only"
        "${_arg_SOURCE_DIR}/apps/charging_only/assets"
    )

    target_compile_definitions("${_arg_TARGET}" PRIVATE CUSTOM_MODES)
endfunction()
