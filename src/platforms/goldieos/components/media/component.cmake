function(goldieos_component_media target)
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/media.c")
endfunction()
