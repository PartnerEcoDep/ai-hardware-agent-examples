include("${CMAKE_CURRENT_LIST_DIR}/../../paths.cmake")
idf_component_register(SRCS "${UNIFIED_SOURCE_ROOT}/platforms/esp_idf/components/ui/ai_ui.c" "${UNIFIED_SOURCE_ROOT}/application/ui/ai_lvgl.c" INCLUDE_DIRS "${UNIFIED_SOURCE_ROOT}/application/ui/include"
    REQUIRES ai_core PRIV_REQUIRES board lvgl esp_timer)
