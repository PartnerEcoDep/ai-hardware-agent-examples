include("${CMAKE_CURRENT_LIST_DIR}/../../paths.cmake")
set(port "${UNIFIED_SOURCE_ROOT}/platforms/esp_idf")
idf_component_register(SRCS "${port}/components/convai/convai_platform_esp32.c" "${port}/components/convai/esp32_osal.c" "${port}/components/convai/esp32_netal.c" "${port}/components/convai/esp32_tlsal.c" "${port}/components/convai/esp32_misc.c"
    INCLUDE_DIRS "${port}/components/convai/include"
    PRIV_INCLUDE_DIRS "${port}/components/convai"
    REQUIRES convai_sdk
    PRIV_REQUIRES esp_timer esp_netif esp_wifi mbedtls lwip nvs_flash)
