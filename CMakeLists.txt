cmake_minimum_required(VERSION 3.10)
include("${CMAKE_CURRENT_LIST_DIR}/cmake/ConvaiVersion.cmake")
project(ai_hardware_agent_examples
    VERSION "${CONVAI_PROJECT_VERSION_BASE}"
    LANGUAGES C CXX)

# ====================================================================
#  Global settings
# ====================================================================

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Enable compile warnings
if(MSVC)
    add_compile_options(/W3)
else()
    add_compile_options(-Wall -Wextra)
endif()

message(STATUS "CONVAI_PROJECT_VERSION = ${CONVAI_PROJECT_VERSION}")

# ====================================================================
#  Examples  (ws63 firmware / win simulator)
# ====================================================================

if(CONVAI_PLATFORM STREQUAL "ws63")
    # ---- C Compile options (RISC-V specific) ----
    add_compile_options(
        -w
        -march=rv32imfc
        -mabi=ilp32f
        --short-enums
        -std=gnu99
        -fasm
        -fcommon
        -Wall -Wextra
        -Winit-self -Wpointer-arith -Wstrict-prototypes
        -fno-strict-aliasing
        -Os
        -fno-unwind-tables
        -ffreestanding
        -fdata-sections -ffunction-sections
        -nostdlib -pipe
        -fno-tree-scev-cprop
        -mpush-pop -msmall-data-limit=0 -fno-ipa-ra
    )
    if(EXISTS ${CMAKE_SOURCE_DIR}/src/CMakeLists.txt)
        add_subdirectory(src)
        target_compile_definitions(convai_sdk PRIVATE
            CONVAI_PROJECT_VERSION="${CONVAI_PROJECT_VERSION}"
        )
    endif()
    add_subdirectory(examples/goldieos)
elseif(CONVAI_PLATFORM STREQUAL "goldieos")
    # ---- Windows simulator (native MinGW gcc/g++) ----
    # Configure with:
    #   cmake -G "MinGW Makefiles" -DCONVAI_PLATFORM=goldieos \
    #         -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
    #         -DCMAKE_C_COMPILER_WORKS=1 -DCMAKE_CXX_COMPILER_WORKS=1 \
    #         -DCMAKE_MAKE_PROGRAM=mingw32-make ..
    message(STATUS "CONVAI_PLATFORM=goldieos: building Windows simulator (goldieos.exe)")
    if(EXISTS ${CMAKE_SOURCE_DIR}/src/CMakeLists.txt)
        add_subdirectory(src)
        target_compile_definitions(convai_sdk PRIVATE
            CONVAI_PROJECT_VERSION="${CONVAI_PROJECT_VERSION}"
        )
    endif()
    add_subdirectory(examples/goldieos)
else()
    message(STATUS "CONVAI_PLATFORM not set or unsupported: '${CONVAI_PLATFORM}'. Supported platforms: ws63, win")
endif()
