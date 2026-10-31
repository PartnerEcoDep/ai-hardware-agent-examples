cmake_minimum_required(VERSION 3.20)
project(unified_ai_ws63_core C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
set(UNIFIED_SOURCE_ROOT "${root}/src")
set(CONVAI_SDK_ROOT "${root}/third_party/convai-sdk-26.8.0" CACHE PATH "ConvAI SDK 26.8.0")
# This builds only the reusable library. The host application supplies LiteOS
# event dispatch, media workers, SDK HAL and its own application presenter.
add_library(unified_ai_portable STATIC
    "${UNIFIED_SOURCE_ROOT}/application/core/ai_app.c"
    "${UNIFIED_SOURCE_ROOT}/application/core/ai_controller.c"
    "${UNIFIED_SOURCE_ROOT}/application/services/cloud/ai_cloud.c"
    "${UNIFIED_SOURCE_ROOT}/application/services/media/ai_audio.c"
    "${UNIFIED_SOURCE_ROOT}/application/services/media/ai_playback.c"
    "${UNIFIED_SOURCE_ROOT}/application/services/network/ai_provision.c"
    "${UNIFIED_SOURCE_ROOT}/application/services/network/ai_wifi_store.c"
    "${UNIFIED_SOURCE_ROOT}/application/services/network/ai_network_status.c")
target_include_directories(unified_ai_portable PUBLIC
    "${UNIFIED_SOURCE_ROOT}/application/core/include" "${UNIFIED_SOURCE_ROOT}/application/services/cloud/include" "${UNIFIED_SOURCE_ROOT}/application/services/media/include"
    "${CONVAI_SDK_ROOT}/include/convai" "${UNIFIED_SOURCE_ROOT}/application/services/network/include")
target_compile_options(unified_ai_portable PRIVATE -Wall -Wextra -Werror)
target_link_libraries(unified_ai_portable INTERFACE "${CONVAI_SDK_ROOT}/libs/ws63/libconvai_sdk.a")
