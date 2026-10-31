include("${CMAKE_CURRENT_LIST_DIR}/../../paths.cmake")
set(port "${UNIFIED_SOURCE_ROOT}/platforms/esp_idf")
idf_component_register(SRCS "${port}/components/config/config.c"
    INCLUDE_DIRS "${port}/components/config/include"
    REQUIRES ai_cloud
    PRIV_REQUIRES esp_sdk cjson)
