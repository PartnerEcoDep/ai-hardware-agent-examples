include("${CMAKE_CURRENT_LIST_DIR}/../../paths.cmake")
set(port "${UNIFIED_SOURCE_ROOT}/platforms/esp_idf")
idf_component_register(SRCS "${port}/components/network/network.cpp"
    INCLUDE_DIRS "${port}/components/network/include"
    PRIV_REQUIRES ai_network esp-wifi-connect esp_timer lwip)
