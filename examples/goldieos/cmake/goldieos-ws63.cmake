include_guard(GLOBAL)

include(CMakeParseArguments)

function(goldieos_add_ws63_target)
    set(_options)
    set(_one_value_args TARGET PLATFORM_TARGET SOURCE_DIR)
    cmake_parse_arguments(GOLDIEOS_WS63
        "${_options}" "${_one_value_args}" "" ${ARGN})

    foreach(_required_arg IN ITEMS TARGET PLATFORM_TARGET SOURCE_DIR)
        if(NOT GOLDIEOS_WS63_${_required_arg})
            message(FATAL_ERROR
                "goldieos_add_ws63_target requires ${_required_arg}")
        endif()
    endforeach()

    if(TARGET "${GOLDIEOS_WS63_TARGET}")
        message(FATAL_ERROR
            "Target already exists: ${GOLDIEOS_WS63_TARGET}")
    endif()
    if(TARGET "${GOLDIEOS_WS63_PLATFORM_TARGET}")
        message(FATAL_ERROR
            "Target already exists: ${GOLDIEOS_WS63_PLATFORM_TARGET}")
    endif()

    set(_source_dir "${GOLDIEOS_WS63_SOURCE_DIR}")
    set(_mbedtls_dir "${_source_dir}/third_party/mbedtls-ws63")
    set(_platform_include_dirs
        "${_source_dir}/include/platform/ws63"
        "${_source_dir}/include/platform/ws63/bts/sle"
        "${_source_dir}/include/platform/ws63/bts/ble"
        "${_source_dir}/include/platform/ws63/bts/common"
        "${_source_dir}/include/platform/ws63/slp"
        "${_source_dir}/include/third_party/lwip"
        "${_source_dir}/third_party/speex_aec"
        "${_source_dir}/third_party/speex_aec/include"
        "${_source_dir}/drivers/codec/es8311_drv"
        "${_source_dir}/drivers/lcd"
    )

    add_library("${GOLDIEOS_WS63_PLATFORM_TARGET}" INTERFACE)
    target_compile_definitions("${GOLDIEOS_WS63_PLATFORM_TARGET}" INTERFACE
        PLATFORM_TYPE_WS63
        ST7789_SPI_LCD
        CONFIG_SUPPORT_ES8311_CODEC
        SUPPORT_EXT_GPIO_PA
        SUPPORT_SLE
        __EMBEDDED__
        HAVE_CONFIG_H
        FIXED_POINT
        USE_KISS_FFT
        SUPPORT_BATTERY
        SUPPORT_GPIO_KEYBOARD
        LCM_USE_EXT_GPIO
        SUPPORT_CST816D_TOUCH
        USE_EXT_ASR_CHIP_AC2817
        SUPPORT_PCF8563_RTC
        DRV_CORE
        CONFIG_ROM_COMPILE
        INLINE_TO_FORCEINLINE
        LWIP_CONFIG_FILE="lwipopts_default.h"
        MBEDTLS_CONFIG_FILE="goldieos_config.h"
        MBEDTLS_USER_CONFIG_FILE="mbedtls_platform_hardware_config.h"
        TD_SUPPORT_STDLIB
        CUSTOM_MODES
    )
    target_link_options("${GOLDIEOS_WS63_PLATFORM_TARGET}" INTERFACE
        -static
        -Wl,--enjal16
        -nostdlib
    )

    file(GLOB _platform_sources CONFIGURE_DEPENDS
        "${_source_dir}/platform/ws63/*.c"
        "${_source_dir}/platform/ws63/adc/*.c"
        "${_source_dir}/platform/ws63/sle/*.c"
    )
    list(APPEND _platform_sources
        "${_source_dir}/platform/convai_platform_ws63.c")

    set(_driver_sources
        "${_source_dir}/drivers/lcd/st7789.c"
        "${_source_dir}/drivers/touch/cst816d.c"
        "${_source_dir}/drivers/codec/es8311_drv/es8311.c"
        "${_source_dir}/drivers/keyboard/gpio_keyboard.c"
        "${_source_dir}/drivers/i2c/i2c.c"
        "${_source_dir}/drivers/rtc/pcf8563.c"
        "${_source_dir}/drivers/battery/bat_driver.c"
        "${_source_dir}/drivers/extern_io/aw9523b.c"
        "${_source_dir}/drivers/audio_pa/pa_drv.c"
    )

    file(GLOB _sle_service_sources CONFIGURE_DEPENDS
        "${_source_dir}/services/sle_service/*.c")
    file(GLOB _speex_sources CONFIGURE_DEPENDS
        "${_source_dir}/third_party/speex_aec/libspeexdsp/*.c")

    add_library("${GOLDIEOS_WS63_TARGET}" STATIC)

    goldieos_target_add_common(
        TARGET "${GOLDIEOS_WS63_TARGET}"
        SOURCE_DIR "${_source_dir}"
        AFTER_INIT_SOURCES
            ${_platform_sources}
            ${_driver_sources}
        AFTER_SERVICE_SOURCES
            ${_sle_service_sources}
        FINAL_SOURCES
            ${_speex_sources}
        PLATFORM_INCLUDE_DIRS
            ${_platform_include_dirs}
    )

    # The WS63 mbedTLS configuration must win over the legacy compatibility
    # headers included by the shared GoldieOS source set.
    target_include_directories("${GOLDIEOS_WS63_TARGET}" BEFORE PRIVATE
        "${_mbedtls_dir}/include"
        "${_mbedtls_dir}/configs"
        "${_mbedtls_dir}/harden/src/internal_include"
        "${_mbedtls_dir}/harden/cipher_adapt_include"
        "${_mbedtls_dir}/harden/platform/connect"
    )
    target_link_libraries("${GOLDIEOS_WS63_TARGET}" PRIVATE
        "${GOLDIEOS_WS63_PLATFORM_TARGET}"
        opus
        convai_sdk
        -lc
    )

    convai_target_enable_warnings("${GOLDIEOS_WS63_TARGET}")
    convai_target_enable_ws63_options("${GOLDIEOS_WS63_TARGET}")
endfunction()
