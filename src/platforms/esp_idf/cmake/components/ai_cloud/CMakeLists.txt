include("${CMAKE_CURRENT_LIST_DIR}/../../paths.cmake")
idf_component_register(SRCS "${UNIFIED_SOURCE_ROOT}/application/services/cloud/ai_cloud.c"
    INCLUDE_DIRS "${UNIFIED_SOURCE_ROOT}/application/services/cloud/include" REQUIRES ai_core ai_media PRIV_REQUIRES convai_sdk)
