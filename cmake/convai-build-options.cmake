# Target-scoped build policy shared by the standalone CMake entry points.

include_guard(GLOBAL)

set(_convai_warning_options
    "$<$<COMPILE_LANG_AND_ID:C,MSVC>:/W3>"
    "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/W3>"
    "$<$<COMPILE_LANG_AND_ID:C,GNU,Clang,AppleClang>:-Wall;-Wextra>"
    "$<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang,AppleClang>:-Wall;-Wextra>"
)

if(NOT TARGET convai_warnings)
    add_library(convai_warnings INTERFACE)
    add_library(convai::warnings ALIAS convai_warnings)

    target_compile_options(convai_warnings INTERFACE
        ${_convai_warning_options})
endif()

set(_convai_ws63_compile_options
    "$<$<COMPILE_LANGUAGE:C>:-fasm>"
    "$<$<COMPILE_LANGUAGE:C>:-fcommon>"
    -Winit-self
    -Wpointer-arith
    "$<$<COMPILE_LANGUAGE:C>:-Wstrict-prototypes>"
    -fno-strict-aliasing
    -fno-unwind-tables
    -ffreestanding
    -fdata-sections
    -ffunction-sections
    -pipe
    -fno-tree-scev-cprop
    -mpush-pop
    -msmall-data-limit=0
    -fno-ipa-ra
    "$<$<CONFIG:Release>:-Os>"
    "$<$<CONFIG:RelWithDebInfo>:-Os>"
    "$<$<CONFIG:Debug>:-Og>"
    "$<$<CONFIG:Debug>:-g3>"
)

if(NOT TARGET convai_ws63_options)
    add_library(convai_ws63_options INTERFACE)
    add_library(convai::ws63-options ALIAS convai_ws63_options)

    # ABI flags and compiler paths belong to the WS63 toolchain. These are the
    # project build-policy flags formerly applied globally by the root project.
    target_compile_options(convai_ws63_options INTERFACE
        ${_convai_ws63_compile_options})
endif()

function(convai_target_enable_warnings target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR
            "convai_target_enable_warnings: target does not exist: ${target}")
    endif()

    get_target_property(_actual_target "${target}" ALIASED_TARGET)
    if(NOT _actual_target)
        set(_actual_target "${target}")
    endif()

    # Apply the options directly so exportable third-party targets (notably
    # Opus) do not acquire a dependency on this project's policy target.
    target_compile_options("${_actual_target}" PRIVATE
        ${_convai_warning_options})
endfunction()

function(convai_target_enable_ws63_options target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR
            "convai_target_enable_ws63_options: target does not exist: ${target}")
    endif()

    get_target_property(_actual_target "${target}" ALIASED_TARGET)
    if(NOT _actual_target)
        set(_actual_target "${target}")
    endif()

    # Target properties are required here: an INTERFACE library cannot
    # propagate the C_EXTENSIONS property itself.
    set_target_properties("${_actual_target}" PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS ON
    )
    target_compile_options("${_actual_target}" PRIVATE
        ${_convai_ws63_compile_options})
endfunction()
