function(goldieos_component_runtime target backend)
    target_sources(${target} PRIVATE "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/${backend}/runtime.c")
endfunction()
