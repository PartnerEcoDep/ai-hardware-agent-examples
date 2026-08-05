# ====================================================================
#  goldieos — dual-platform build (selected via CONVAI_PLATFORM)
#
#  CONVAI_PLATFORM=ws63 : HiSilicon WS63 firmware (RISC-V 32-bit + FPU,
#                         LiteOS, lwIP). Outputs goldieos.fwpkg for flashing.
#  CONVAI_PLATFORM=goldieos  : Windows simulator (native MinGW gcc/g++).
#                         Outputs goldieos.exe, runs directly on PC.
# ====================================================================

cmake_minimum_required(VERSION 3.10)

# ---- Toolchain ——
# Specify via command line, matching old project conventions:
#   cmake -G "MinGW Makefiles" -DCONVAI_PLATFORM=ws63 \
#         -DCMAKE_C_COMPILER=riscv32-linux-musl-gcc \
#         -DCMAKE_CXX_COMPILER=riscv32-linux-musl-g++ \
#         -DCMAKE_C_COMPILER_WORKS=1 -DCMAKE_CXX_COMPILER_WORKS=1 \
#         -DCMAKE_MAKE_PROGRAM=make ..
#   cmake -G "MinGW Makefiles" -DCONVAI_PLATFORM=goldieos \
#         -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
#         -DCMAKE_C_COMPILER_WORKS=1 -DCMAKE_CXX_COMPILER_WORKS=1 \
#         -DCMAKE_MAKE_PROGRAM=mingw32-make ..
# Add the toolchain bin directory to PATH before invoking cmake.

# ====================================================================
#  Common — shared headers, sources, and third-party code
# ====================================================================

set(COMMON_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/sdk_integration
    ${CMAKE_CURRENT_SOURCE_DIR}/compat
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/core
    ${CMAKE_CURRENT_SOURCE_DIR}/include/osal
    ${CMAKE_CURRENT_SOURCE_DIR}/include/services
    ${CMAKE_CURRENT_SOURCE_DIR}/include/services/alarm
    ${CMAKE_CURRENT_SOURCE_DIR}/include/services/ntp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/services/audio
    ${CMAKE_CURRENT_SOURCE_DIR}/include/system_res
    ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party/cjson
    ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party/mbedtls
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/fatfs-R0.11/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/webrtc_vad
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/helix/real
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/helix/pub
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/opus-1.6.1/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/gui
)

# Init (system start-up)
set(COMMON_INIT_SRC init/system_init.c)

# Service sources (shared)
file(GLOB COMMON_SERVICE_SRC
    services/alarm_service/*.c
    services/ntp_service/*.c
    services/aud_algo/*.c
)

# SDK integration (replaces cloud_service)
file(GLOB COMMON_SDK_INTEGRATION_SRC sdk_integration/*.c)

# App sources
file(GLOB COMMON_APP_SRC
    apps/launcher/*.cpp
    apps/settings/*.cpp
    apps/alarm/*.cpp
    apps/animaton_player/*.cpp
    # apps/recorder/*.cpp
    # apps/AItalk/*.cpp
    # apps/Walkie-talkie/*.cpp
    # apps/Story/*.cpp
    # apps/Story/*.c
    apps/shut_down/*.cpp
    apps/charging_only/*.cpp
)

# App-specific include dirs (app source + assets)
set(COMMON_APP_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/launcher
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/launcher/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/animaton_player
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/animaton_player/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/recorder
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/recorder/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/AItalk
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/AItalk/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/settings
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/settings/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/alarm
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/alarm/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/shut_down
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/shut_down/assets
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/charging_only
    ${CMAKE_CURRENT_SOURCE_DIR}/apps/charging_only/assets
)

# Third-party sources (compiled from source; shared)
file(GLOB COMMON_THIRDPARTY_SRC
    third_party/fatfs-R0.11/src/*.c
    third_party/fatfs-R0.11/src/option/*.c
    third_party/webrtc_vad/*.c
    third_party/webrtc_vad/signal_processing/*.c
    third_party/webrtc_vad/vad/*.c
    third_party/helix/*.c
    third_party/helix/real/*.c
)

# ---- Opus codec (shared configuration) ----
set(OPUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/opus-1.6.1)
if(EXISTS ${OPUS_DIR}/CMakeLists.txt)
    add_compile_definitions(CUSTOM_MODES)
    set(OPUS_VAR_ARRAYS OFF CACHE BOOL "Disable variable length arrays" FORCE)
    set(OPUS_USE_ALLOCA OFF CACHE BOOL "Disable alloca" FORCE)
    set(OPUS_FIXED_POINT ON CACHE BOOL "Enable fixed-point" FORCE)
    set(OPUS_ASM OFF CACHE BOOL "Disable assembly" FORCE)
    set(OPUS_INTRINSICS OFF CACHE BOOL "Disable intrinsics" FORCE)
    set(OPUS_DISABLE_FLOAT_API ON CACHE BOOL "Disable float API" FORCE)
    set(OPUS_ENABLE_FLOAT_API OFF CACHE BOOL "Enable float API" FORCE)
    set(OPUS_CUSTOM_MODES ON CACHE BOOL "Enable custom modes" FORCE)

    if(CONVAI_PLATFORM STREQUAL "ws63")
        set(OPUS_DRED OFF CACHE BOOL "Disable DRED" FORCE)
        set(OPUS_OSCE OFF CACHE BOOL "Disable OSCE" FORCE)
        set(OPUS_FORTIFY_SOURCE OFF CACHE BOOL "Disable fortify" FORCE)
    elseif(CONVAI_PLATFORM STREQUAL "goldieos")
        set(OPUS_NONTHREADSAFE_PSEUDOSTACK ON CACHE BOOL "Enable pseudostack" FORCE)
        set(OPUS_CUSTOM_API ON CACHE BOOL "Enable custom API" FORCE)
        set(OPUS_X86_MAY_HAVE_SSE     OFF CACHE BOOL "Disable SSE" FORCE)
        set(OPUS_X86_MAY_HAVE_SSE2    OFF CACHE BOOL "Disable SSE2" FORCE)
        set(OPUS_X86_MAY_HAVE_SSE4_1  OFF CACHE BOOL "Disable SSE4.1" FORCE)
        set(OPUS_X86_MAY_HAVE_AVX2    OFF CACHE BOOL "Disable AVX2" FORCE)
        set(OPUS_X86_PRESUME_SSE      OFF CACHE BOOL "Disable SSE presume" FORCE)
        set(OPUS_X86_PRESUME_SSE2     OFF CACHE BOOL "Disable SSE2 presume" FORCE)
        set(OPUS_X86_PRESUME_SSE4_1   OFF CACHE BOOL "Disable SSE4.1 presume" FORCE)
        set(OPUS_X86_PRESUME_AVX2     OFF CACHE BOOL "Disable AVX2 presume" FORCE)
    endif()

    add_subdirectory(${OPUS_DIR} opus_build)
endif()


# ====================================================================
#  WS63 — HiSilicon WS63 firmware (RISC-V 32-bit + FPU, LiteOS, lwIP)
# ====================================================================
if(CONVAI_PLATFORM STREQUAL "ws63")

    # -- Platform-specific include dirs --
    set(WS63_INCLUDE_DIRS
        # Platform headers (WS63 hardware)
        ${CMAKE_CURRENT_SOURCE_DIR}/include/platform/ws63
        ${CMAKE_CURRENT_SOURCE_DIR}/include/platform/ws63/bts/sle
        ${CMAKE_CURRENT_SOURCE_DIR}/include/platform/ws63/bts/ble
        ${CMAKE_CURRENT_SOURCE_DIR}/include/platform/ws63/bts/common
        ${CMAKE_CURRENT_SOURCE_DIR}/include/platform/ws63/slp

        # Third-party headers (WS63 only)
        ${CMAKE_CURRENT_SOURCE_DIR}/include/third_party/lwip
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/speex_aec
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/speex_aec/include

        # Driver headers
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/codec/es8311_drv
        ${CMAKE_CURRENT_SOURCE_DIR}/drivers/lcd
    )

    # -- Platform sources --
    file(GLOB WS63_PLATFORM_SRC
        platform/ws63/*.c
        platform/ws63/adc/*.c
        platform/ws63/sle/*.c
        platform/ws63/stubs.c
    )
    list(APPEND WS63_PLATFORM_SRC platform/convai_platform_ws63.c)

    # -- Driver sources (WS63 only) --
    set(WS63_DRIVER_SRC
        drivers/lcd/st7789.c
        drivers/touch/cst816d.c
        drivers/codec/es8311_drv/es8311.c
        drivers/keyboard/gpio_keyboard.c
        drivers/i2c/i2c.c
        drivers/rtc/pcf8563.c
        drivers/battery/bat_driver.c
        drivers/extern_io/aw9523b.c
        drivers/audio_pa/pa_drv.c
    )

    # SLE service (WS63 only)
    file(GLOB WS63_SLE_SERVICE_SRC services/sle_service/*.c)

    # Speex AEC (WS63 only)
    file(GLOB WS63_SPEEX_SRC third_party/speex_aec/libspeexdsp/*.c)

    # -- Compile definitions (WS63 hardware) --
    add_compile_definitions(
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
    )

    # -- Link options --
    add_link_options(
        -static
        -Wl,--enjal16
        -nostdlib
    )

    # -- SDK library: source-built (this repo) or prebuilt (donor layout) --
    # Source build creates the convai_sdk target via add_subdirectory(src) in the
    # root CMakeLists (present only when src/ exists); otherwise fall back to the
    # prebuilt archive. Auto-detected — no flag needed.
    if(NOT TARGET convai_sdk)
        if(NOT EXISTS ${CMAKE_SOURCE_DIR}/libs/ws63/libconvai_sdk.a)
            message(FATAL_ERROR
                "未找到 WS63 版 convai_sdk 库: ${CMAKE_SOURCE_DIR}/libs/ws63/libconvai_sdk.a\n"
                "源码构建需存在 src/CMakeLists.txt；预编译方式请将该 .a 放入 libs/ws63/。")
        endif()
        add_library(convai_sdk STATIC IMPORTED)
        set_target_properties(convai_sdk PROPERTIES
            IMPORTED_LOCATION ${CMAKE_SOURCE_DIR}/libs/ws63/libconvai_sdk.a
        )
        message(STATUS "WS63: 使用预编译 convai_sdk: ${WIN_SDK_LIB}")
    else()
        message(STATUS "WS63: 使用源码构建 convai_sdk")
    endif()

    # -- Library target --
    add_library(goldieos_ws63 STATIC
        ${COMMON_INIT_SRC}
        ${WS63_PLATFORM_SRC}
        ${WS63_DRIVER_SRC}
        ${COMMON_SERVICE_SRC}
        ${WS63_SLE_SERVICE_SRC}
        ${COMMON_SDK_INTEGRATION_SRC}
        ${COMMON_APP_SRC}
        ${COMMON_THIRDPARTY_SRC}
        ${WS63_SPEEX_SRC}
    )

    target_include_directories(goldieos_ws63 PRIVATE
        ${COMMON_INCLUDE_DIRS}
        ${WS63_INCLUDE_DIRS}
        ${COMMON_APP_INCLUDE_DIRS}
    )

    target_link_libraries(goldieos_ws63 PRIVATE
        opus
        convai_sdk
        -lc
    )

    # -- Board package path --
    if(WS63_BOARD_ROOT)
        set(WS63_LINK_ROOT "${WS63_BOARD_ROOT}")
    elseif(DEFINED ENV{GOLDIEOS_ROOT})
        set(WS63_LINK_ROOT "$ENV{GOLDIEOS_ROOT}")
    else()
        set(WS63_LINK_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
    endif()

    if(NOT EXISTS "${WS63_LINK_ROOT}")
        message(FATAL_ERROR "WS63 board package not found at: ${WS63_LINK_ROOT}\n"
            "Set WS63_BOARD_ROOT cmake variable or GOLDIEOS_ROOT environment variable.")
    endif()

    # -- Linker step (ws63_link_v4.exe) --
    set(WS63_LINKER    "${WS63_LINK_ROOT}/tools/build/tools/ws63_link_v4.exe")
    set(OUTPUT_DIR     "${CMAKE_CURRENT_BINARY_DIR}/out")
    set(OUTPUT_BIN     "${OUTPUT_DIR}/goldieos.bin")
    set(OUTPUT_ELF     "${OUTPUT_DIR}/goldieos.elf")

    set(WS63_LIB_GOLDIEOS "libgoldieos_ws63.a")
    set(WS63_LIB_OPUS     "opus_build/libopus.a")
    # Resolves to the built archive (source mode) or IMPORTED_LOCATION (prebuilt).
    set(WS63_LIB_SDK      "$<TARGET_FILE:convai_sdk>")

    add_custom_command(
        OUTPUT ${OUTPUT_BIN} ${OUTPUT_ELF}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
        COMMAND ${WS63_LINKER} ${WS63_LINK_ROOT} "${WS63_LIB_GOLDIEOS},${WS63_LIB_OPUS},${WS63_LIB_SDK}" ${OUTPUT_DIR}
        DEPENDS goldieos_ws63 opus convai_sdk
        COMMENT "Generating firmware with ws63_link_v4"
        VERBATIM
    )

    add_custom_target(ws63_firmware ALL
        DEPENDS ${OUTPUT_BIN}
        COMMENT "Firmware: ${OUTPUT_BIN}"
    )

    message(STATUS "GoldieOS WS63 configured. Compiler: ${CMAKE_C_COMPILER}")


# ====================================================================
#  WIN — Windows simulator (native MinGW build)
# ====================================================================
elseif(CONVAI_PLATFORM STREQUAL "goldieos")

    # -- Platform-specific include dirs --
    set(WIN_INCLUDE_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/include/platform/win10
    )

    # -- Platform sources --
    set(WIN_PLATFORM_SRC
        platform/convai_platform_win.c
        platform/win/goldie_osal_win_compat.c
    )

    # cJSON: WS63 gets it from the board prebuilt lib; WIN compiles from source
    file(GLOB WIN_CJSON_SRC third_party/cjson/*.c)

    # -- SDK library: source-built (this repo) or prebuilt (donor layout) --
    # Source build creates the convai_sdk target via add_subdirectory(src) in the
    # root CMakeLists (present only when src/ exists); otherwise fall back to the
    # prebuilt archive. Auto-detected — no flag needed.
    if(NOT TARGET convai_sdk)
        set(WIN_SDK_LIB ${CMAKE_SOURCE_DIR}/libs/win/libconvai_sdk.a)
        if(NOT EXISTS ${WIN_SDK_LIB})
            message(FATAL_ERROR
                "未找到 Windows 版 convai_sdk 库: ${WIN_SDK_LIB}\n"
                "源码构建需存在 src/CMakeLists.txt；预编译方式请获取 Windows MinGW 版 libconvai_sdk.a 并放入 libs/win/。")
        endif()
        add_library(convai_sdk STATIC IMPORTED)
        set_target_properties(convai_sdk PROPERTIES
            IMPORTED_LOCATION ${WIN_SDK_LIB}
        )
        message(STATUS "WIN: 使用预编译 convai_sdk: ${WIN_SDK_LIB}")
    else()
        message(STATUS "WIN: 使用源码构建 convai_sdk")
    endif()

    # -- WIN prebuilt libraries --
    set(WIN10_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libs/win10)

    # -- Compile definitions (WIN simulator) --
    # 不定义 DRV_CORE / SUPPORT_BATTERY / SUPPORT_SLE / __EMBEDDED__ 等硬件特性宏,
    # 电源管理 / SLE / 硬件驱动相关代码自动编译为空.
    add_compile_definitions(
        PLATFORM_TYPE_WIN
        ST7789_SPI_LCD
        TF_LITE_STATIC_MEMORY
        TF_LITE_DISABLE_X86_NEON
        TF_LITE_STRIP_ERROR_STRINGS
    )

    # -- Fix SDK cltq pointer-truncation bug --
    add_link_options(-Wl,--disable-dynamicbase -Wl,--image-base,0x10000000)

    # -- Executable target --
    add_executable(goldieos
        ${COMMON_INIT_SRC}
        ${WIN_PLATFORM_SRC}
        ${COMMON_SERVICE_SRC}
        ${COMMON_SDK_INTEGRATION_SRC}
        ${COMMON_APP_SRC}
        ${COMMON_THIRDPARTY_SRC}
        ${WIN_CJSON_SRC}
    )

    target_include_directories(goldieos PRIVATE
        ${COMMON_INCLUDE_DIRS}
        ${WIN_INCLUDE_DIRS}
        ${COMMON_APP_INCLUDE_DIRS}
    )

    # winvm/tiny_core/tiny_gui 必须 --whole-archive:
    # main()、虚拟驱动、服务/驱动注册等符号仅经 constructor/init 表引用,
    # 常规按需链接会被丢弃, 导致程序启动后无初始化动作.
    set(WIN_LINK_LIBS
        -mconsole
        -static-libgcc -static-libstdc++ -static
        -Wl,--gc-sections
        -Wl,--undefined=__start_goldie_init_table
        -Wl,--undefined=__stop_goldie_init_table
        -Wl,--whole-archive
        ${WIN10_LIB_DIR}/libwinvm.a
        ${WIN10_LIB_DIR}/libtiny_core.a
        ${WIN10_LIB_DIR}/libtiny_gui.a
        -Wl,--no-whole-archive
        convai_sdk
        ${WIN10_LIB_DIR}/libmbedtls.a
        ${WIN10_LIB_DIR}/libmbedx509.a
        ${WIN10_LIB_DIR}/libmbedcrypto.a
        ${WIN10_LIB_DIR}/libaud_algo.a
        ${WIN10_LIB_DIR}/libtflm_lib.a
        winmm
        ws2_32
        gdi32
        user32
    )
    if(TARGET opus)
        list(APPEND WIN_LINK_LIBS opus)
    endif()

    target_link_libraries(goldieos PRIVATE ${WIN_LINK_LIBS})

    # -- Runtime config --
    add_custom_command(TARGET goldieos POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_SOURCE_DIR}/sdk_integration/convai.cfg.example
            $<TARGET_FILE_DIR:goldieos>/convai.cfg.example
        COMMAND if not exist \"$<TARGET_FILE_DIR:goldieos>/convai.cfg\"
            ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/sdk_integration/convai.cfg.example
            \"$<TARGET_FILE_DIR:goldieos>/convai.cfg\"
        COMMENT "Copying runtime config template (convai.cfg)"
    )

    message(STATUS "GoldieOS WIN simulator configured. Compiler: ${CMAKE_C_COMPILER}")

endif()
