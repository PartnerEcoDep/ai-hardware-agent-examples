idf_build_get_property(CONVAI_SDK_ROOT CONVAI_SDK_ROOT)
get_filename_component(sdk_root "${CONVAI_SDK_ROOT}" ABSOLUTE)
set(sdk_lib "${sdk_root}/libs/esp32s3/libconvai_sdk.a")
if(NOT EXISTS "${sdk_lib}" OR NOT EXISTS "${sdk_root}/include/convai/convai_api.h")
    message(FATAL_ERROR "Missing real ConvAI SDK at ${sdk_root}; set CONVAI_SDK_ROOT. No placeholder engine is provided.")
endif()
idf_component_register(INCLUDE_DIRS "${sdk_root}/include/convai" REQUIRES cjson)
# SDK 26.8.0 already namespaces its embedded JSON implementation. Some public
# cJSON references still resolve against the normal IDF cJSON component.
add_library(convai_binary STATIC IMPORTED GLOBAL)
set_target_properties(convai_binary PROPERTIES IMPORTED_LOCATION "${sdk_lib}")
target_link_libraries(${COMPONENT_LIB} INTERFACE convai_binary)
