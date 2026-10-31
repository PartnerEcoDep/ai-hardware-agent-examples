cmake_minimum_required(VERSION 3.20)
project(unified_ai_tests C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
enable_testing()
add_executable(test_dhcp_gateway test_dhcp_gateway.c ../src/platforms/goldieos/components/network/ws63/dhcp_gateway.c)
target_include_directories(test_dhcp_gateway PRIVATE ../src/platforms/goldieos/components/network/ws63)
add_test(NAME ws63_dhcp_gateway COMMAND test_dhcp_gateway)
add_executable(test_portal test_portal.c ../src/application/services/network/ai_portal.c)
target_include_directories(test_portal PRIVATE ../src/application/services/network/include)
add_test(NAME portal_dns_protocol COMMAND test_portal)
add_compile_options(-Wall -Wextra -Werror -UNDEBUG)
set(root "${CMAKE_CURRENT_LIST_DIR}/..")
set(UNIFIED_SOURCE_ROOT "${root}/src")
include("${UNIFIED_SOURCE_ROOT}/platforms/goldieos/cmake/runtime.cmake")
set(bsp "${GOLDIEOS_ROOT}")
add_executable(test_core test_core.c "${UNIFIED_SOURCE_ROOT}/application/core/ai_app.c")
target_include_directories(test_core PRIVATE "${UNIFIED_SOURCE_ROOT}/application/core/include")
add_test(NAME core_lifecycle COMMAND test_core)
add_executable(test_audio test_audio.c "${UNIFIED_SOURCE_ROOT}/application/services/media/ai_audio.c" "${UNIFIED_SOURCE_ROOT}/application/services/media/ai_playback.c")
target_include_directories(test_audio PRIVATE "${UNIFIED_SOURCE_ROOT}/application/services/media/include")
add_test(NAME audio_mapping_buffer COMMAND test_audio)
add_executable(test_cloud test_cloud.c "${UNIFIED_SOURCE_ROOT}/application/services/cloud/ai_cloud.c")
target_include_directories(test_cloud PRIVATE "${UNIFIED_SOURCE_ROOT}/application/core/include" "${UNIFIED_SOURCE_ROOT}/application/services/cloud/include" "${UNIFIED_SOURCE_ROOT}/application/services/media/include"
    "${root}/third_party/convai-sdk-26.8.0/include/convai")
add_test(NAME cloud_lifecycle COMMAND test_cloud)

add_executable(test_provision test_provision.c "${UNIFIED_SOURCE_ROOT}/application/services/network/ai_provision.c")
target_include_directories(test_provision PRIVATE "${UNIFIED_SOURCE_ROOT}/application/services/network/include")
add_test(NAME provision_validation COMMAND test_provision)
add_executable(test_ws_wifi_store test_ws_wifi_store.c
    "${UNIFIED_SOURCE_ROOT}/platforms/goldieos/components/network/ws63/wifi_store.c" "${UNIFIED_SOURCE_ROOT}/application/services/network/ai_provision.c")
target_include_directories(test_ws_wifi_store PRIVATE "${UNIFIED_SOURCE_ROOT}/platforms/goldieos/components/network/ws63"
    "${UNIFIED_SOURCE_ROOT}/application/services/network/include"
    "${bsp}/include/platform/ws63")
add_test(NAME ws63_nv_storage COMMAND test_ws_wifi_store)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/config_fixture")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/config_fixture/ws63_config.h" [=[
#define WS_AI_AGENT_ID "test-agent"
#define WS_AI_API_KEY ""
#define WS_AI_PRODUCT_ID "test-product"
#define WS_AI_PRODUCT_KEY "test-key"
#define WS_AI_PRODUCT_SECRET "test-secret"
#define WS_AI_SERVER_URL ""
]=])
add_executable(test_ws_config test_ws_config.c "${UNIFIED_SOURCE_ROOT}/platforms/goldieos/components/config/config.c"
    "${bsp}/third_party/cjson/cJSON.c")
target_include_directories(test_ws_config PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/config_fixture"
    "${UNIFIED_SOURCE_ROOT}/platforms/goldieos/components/config/include" "${UNIFIED_SOURCE_ROOT}/platforms/goldieos/components/runtime/include" "${UNIFIED_SOURCE_ROOT}/application/core/include" "${UNIFIED_SOURCE_ROOT}/application/services/cloud/include" "${UNIFIED_SOURCE_ROOT}/application/services/media/include"
    "${bsp}/include/third_party/cjson")
add_test(NAME ws63_cloud_config COMMAND test_ws_config)
find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
    add_test(NAME wifi_web_contract COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/test_wifi_web.cjs")
endif()

add_executable(test_controller test_controller.c "${UNIFIED_SOURCE_ROOT}/application/core/ai_app.c" "${UNIFIED_SOURCE_ROOT}/application/core/ai_controller.c")
target_include_directories(test_controller PRIVATE "${UNIFIED_SOURCE_ROOT}/application/core/include")
add_test(NAME application_coordination COMMAND test_controller)

add_executable(test_network_contract test_network_contract.c
    "${UNIFIED_SOURCE_ROOT}/application/services/network/ai_wifi_store.c" "${UNIFIED_SOURCE_ROOT}/application/services/network/ai_provision.c"
    "${UNIFIED_SOURCE_ROOT}/application/services/network/ai_network_status.c")
target_include_directories(test_network_contract PRIVATE "${UNIFIED_SOURCE_ROOT}/application/services/network/include")
add_test(NAME network_contract COMMAND test_network_contract)
add_executable(test_esp_wifi_store test_esp_wifi_store.c
    "${UNIFIED_SOURCE_ROOT}/platforms/esp_idf/components/network/esp-wifi-connect/esp_wifi_store.c"
    "${UNIFIED_SOURCE_ROOT}/application/services/network/ai_provision.c")
target_include_directories(test_esp_wifi_store PRIVATE stubs "${UNIFIED_SOURCE_ROOT}/application/services/network/include"
    "${UNIFIED_SOURCE_ROOT}/platforms/esp_idf/components/network/esp-wifi-connect/include")
add_test(NAME esp_nvs_storage COMMAND test_esp_wifi_store)

add_executable(test_goldie_media test_goldie_media.c
    "${UNIFIED_SOURCE_ROOT}/application/services/media/ai_audio.c"
    "${UNIFIED_SOURCE_ROOT}/application/services/media/ai_playback.c")
target_compile_definitions(test_goldie_media PRIVATE WS_AI_PLAYBACK_BYTES=16000)
target_include_directories(test_goldie_media PRIVATE stubs/goldie_media
    "${CMAKE_CURRENT_BINARY_DIR}/config_fixture" "${bsp}/include/core"
    "${UNIFIED_SOURCE_ROOT}/application/core/include"
    "${UNIFIED_SOURCE_ROOT}/application/services/media/include"
    "${UNIFIED_SOURCE_ROOT}/platforms/goldieos/components/media/include"
    "${UNIFIED_SOURCE_ROOT}/platforms/goldieos/include" "${UNIFIED_SOURCE_ROOT}/boards/ws63/include")
add_test(NAME goldie_media_pipeline COMMAND test_goldie_media)

add_executable(test_ws_delivery_diag test_ws_delivery_diag.c)
add_test(NAME ws63_tls_delivery_diag COMMAND test_ws_delivery_diag)
