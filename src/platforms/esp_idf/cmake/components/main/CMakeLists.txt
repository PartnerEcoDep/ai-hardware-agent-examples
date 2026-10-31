include("${CMAKE_CURRENT_LIST_DIR}/../../paths.cmake")
idf_component_register(SRCS "${UNIFIED_SOURCE_ROOT}/platforms/esp_idf/entry/app_main.c" INCLUDE_DIRS "."
    REQUIRES ai_core ai_cloud ai_ui board esp_media esp_network esp_config esp_sdk esp_timer)
