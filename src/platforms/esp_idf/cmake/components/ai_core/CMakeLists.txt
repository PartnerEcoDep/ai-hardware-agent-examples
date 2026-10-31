include("${CMAKE_CURRENT_LIST_DIR}/../../paths.cmake")
idf_component_register(SRCS "${UNIFIED_SOURCE_ROOT}/application/core/ai_app.c" "${UNIFIED_SOURCE_ROOT}/application/core/ai_controller.c" INCLUDE_DIRS "${UNIFIED_SOURCE_ROOT}/application/core/include")
