# Native MinGW GCC toolchain for the GoldieOS Windows simulator.
#
# By default the tools are resolved from PATH. Set GOLDIEOS_MINGW_ROOT to a
# MinGW installation root (the directory containing bin/) to pin a specific
# installation without committing a machine-specific absolute path.

if(NOT CMAKE_HOST_WIN32)
    message(FATAL_ERROR
        "The GoldieOS Windows simulator toolchain requires a Windows host.")
endif()

set(_GOLDIEOS_DEFAULT_MINGW_ROOT "")
if(DEFINED ENV{GOLDIEOS_MINGW_ROOT} AND NOT "$ENV{GOLDIEOS_MINGW_ROOT}" STREQUAL "")
    set(_GOLDIEOS_DEFAULT_MINGW_ROOT "$ENV{GOLDIEOS_MINGW_ROOT}")
endif()

set(GOLDIEOS_MINGW_ROOT "${_GOLDIEOS_DEFAULT_MINGW_ROOT}" CACHE PATH
    "MinGW installation root for the GoldieOS Windows simulator")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES GOLDIEOS_MINGW_ROOT)

if(GOLDIEOS_MINGW_ROOT)
    set(_GOLDIEOS_MINGW_BIN "${GOLDIEOS_MINGW_ROOT}/bin")
    set(_GOLDIEOS_GCC "${_GOLDIEOS_MINGW_BIN}/gcc.exe")
    set(_GOLDIEOS_GXX "${_GOLDIEOS_MINGW_BIN}/g++.exe")
    set(_GOLDIEOS_AR "${_GOLDIEOS_MINGW_BIN}/ar.exe")
    set(_GOLDIEOS_RANLIB "${_GOLDIEOS_MINGW_BIN}/ranlib.exe")
    set(_GOLDIEOS_WINDRES "${_GOLDIEOS_MINGW_BIN}/windres.exe")
    set(_GOLDIEOS_MAKE "${_GOLDIEOS_MINGW_BIN}/mingw32-make.exe")

    foreach(_GOLDIEOS_REQUIRED_TOOL IN ITEMS
            "${_GOLDIEOS_GCC}"
            "${_GOLDIEOS_GXX}"
            "${_GOLDIEOS_AR}"
            "${_GOLDIEOS_RANLIB}"
            "${_GOLDIEOS_WINDRES}"
            "${_GOLDIEOS_MAKE}")
        if(NOT EXISTS "${_GOLDIEOS_REQUIRED_TOOL}")
            message(FATAL_ERROR
                "MinGW tool not found: ${_GOLDIEOS_REQUIRED_TOOL}\n"
                "Set GOLDIEOS_MINGW_ROOT to the directory containing bin/.")
        endif()
    endforeach()

    set(CMAKE_MAKE_PROGRAM "${_GOLDIEOS_MAKE}" CACHE FILEPATH
        "GoldieOS MinGW make program")
else()
    set(_GOLDIEOS_GCC gcc)
    set(_GOLDIEOS_GXX g++)
    set(_GOLDIEOS_AR ar)
    set(_GOLDIEOS_RANLIB ranlib)
    set(_GOLDIEOS_WINDRES windres)
endif()

# Do not set CMAKE_SYSTEM_NAME here. This is a native Windows build; setting it
# manually would make CMake treat the build as cross-compilation.
set(CMAKE_C_COMPILER "${_GOLDIEOS_GCC}" CACHE FILEPATH
    "GoldieOS MinGW C compiler")
set(CMAKE_CXX_COMPILER "${_GOLDIEOS_GXX}" CACHE FILEPATH
    "GoldieOS MinGW C++ compiler")
set(CMAKE_AR "${_GOLDIEOS_AR}" CACHE FILEPATH
    "GoldieOS MinGW archiver")
set(CMAKE_RANLIB "${_GOLDIEOS_RANLIB}" CACHE FILEPATH
    "GoldieOS MinGW archive indexer")
set(CMAKE_RC_COMPILER "${_GOLDIEOS_WINDRES}" CACHE FILEPATH
    "GoldieOS MinGW resource compiler")

unset(_GOLDIEOS_DEFAULT_MINGW_ROOT)
unset(_GOLDIEOS_MINGW_BIN)
unset(_GOLDIEOS_GCC)
unset(_GOLDIEOS_GXX)
unset(_GOLDIEOS_AR)
unset(_GOLDIEOS_RANLIB)
unset(_GOLDIEOS_WINDRES)
unset(_GOLDIEOS_MAKE)
unset(_GOLDIEOS_REQUIRED_TOOL)
