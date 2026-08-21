# WS63 RISC-V GCC toolchain.
#
# The bundled firmware linker is a Windows executable, so the complete WS63
# build currently requires a Windows host. Override WS63_TOOLCHAIN_ROOT when a
# compatible compiler is installed outside this repository.

if(NOT CMAKE_HOST_WIN32)
    message(FATAL_ERROR
        "The WS63 build currently requires a Windows host because "
        "ws63_link_v4.exe is Windows-only.")
endif()

get_filename_component(_WS63_REPOSITORY_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(_WS63_DEFAULT_TOOLCHAIN_ROOT
    "${_WS63_REPOSITORY_ROOT}/examples/goldieos/tools/build/tools/compiler/riscv/cc_riscv32_musl_105/cc_riscv32_musl_fp_win")

if(DEFINED ENV{WS63_TOOLCHAIN_ROOT} AND NOT "$ENV{WS63_TOOLCHAIN_ROOT}" STREQUAL "")
    set(_WS63_DEFAULT_TOOLCHAIN_ROOT "$ENV{WS63_TOOLCHAIN_ROOT}")
endif()

set(WS63_TOOLCHAIN_ROOT "${_WS63_DEFAULT_TOOLCHAIN_ROOT}" CACHE PATH
    "Root directory of the WS63 RISC-V GCC toolchain")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES WS63_TOOLCHAIN_ROOT)
set(_WS63_TOOLCHAIN_BIN "${WS63_TOOLCHAIN_ROOT}/bin")
set(_WS63_TOOL_PREFIX "${_WS63_TOOLCHAIN_BIN}/riscv32-linux-musl")

foreach(_WS63_REQUIRED_TOOL IN ITEMS gcc g++ ar ranlib objcopy size)
    if(NOT EXISTS "${_WS63_TOOL_PREFIX}-${_WS63_REQUIRED_TOOL}.exe")
        message(FATAL_ERROR
            "WS63 tool not found: ${_WS63_TOOL_PREFIX}-${_WS63_REQUIRED_TOOL}.exe\n"
            "Set WS63_TOOLCHAIN_ROOT to the directory containing bin/.")
    endif()
endforeach()

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv32)

# CMake's compiler probe must not try to link a host executable when cross
# compiling. This replaces the unsafe CMAKE_<LANG>_COMPILER_WORKS overrides.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER "${_WS63_TOOL_PREFIX}-gcc.exe" CACHE FILEPATH
    "WS63 C compiler")
set(CMAKE_CXX_COMPILER "${_WS63_TOOL_PREFIX}-g++.exe" CACHE FILEPATH
    "WS63 C++ compiler")
set(CMAKE_AR "${_WS63_TOOL_PREFIX}-ar.exe" CACHE FILEPATH
    "WS63 archiver")
set(CMAKE_RANLIB "${_WS63_TOOL_PREFIX}-ranlib.exe" CACHE FILEPATH
    "WS63 archive indexer")
set(CMAKE_OBJCOPY "${_WS63_TOOL_PREFIX}-objcopy.exe" CACHE FILEPATH
    "WS63 object copy tool")
set(CMAKE_SIZE "${_WS63_TOOL_PREFIX}-size.exe" CACHE FILEPATH
    "WS63 size tool")

# These flags are part of the WS63 ABI and must also be present in CMake's
# try_compile checks. Build policy flags remain in the project CMakeLists.txt.
set(_WS63_ABI_FLAGS "-march=rv32imfc -mabi=ilp32f --short-enums")
set(CMAKE_C_FLAGS_INIT "${_WS63_ABI_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${_WS63_ABI_FLAGS}")

# GCC 7.3's Windows build forms its implicit libstdc++ search paths with a
# long chain of ".." components. In a deeply nested checkout that raw path can
# exceed the legacy Windows path limit even when the normalized path does not.
# Add the normalized directories explicitly so standard C++ headers remain
# usable without requiring the repository to be moved.
set(_WS63_CXX_INCLUDE_ROOT
    "${WS63_TOOLCHAIN_ROOT}/riscv32-linux-musl/include/c++/7.3.0")
set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
    "${_WS63_CXX_INCLUDE_ROOT}"
    "${_WS63_CXX_INCLUDE_ROOT}/riscv32-linux-musl")

unset(_WS63_ABI_FLAGS)
unset(_WS63_CXX_INCLUDE_ROOT)
unset(_WS63_DEFAULT_TOOLCHAIN_ROOT)
unset(_WS63_REPOSITORY_ROOT)
unset(_WS63_REQUIRED_TOOL)
unset(_WS63_TOOLCHAIN_BIN)
unset(_WS63_TOOL_PREFIX)
