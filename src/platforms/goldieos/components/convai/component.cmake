function(goldieos_component_sdk target backend)
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${backend}/sdk_platform.c")
    if(backend STREQUAL "ws63")
        set_property(SOURCE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ws63/sdk_platform.c"
            APPEND PROPERTY COMPILE_OPTIONS "-I${CMAKE_CURRENT_FUNCTION_LIST_DIR}/ws63")
    endif()
endfunction()
