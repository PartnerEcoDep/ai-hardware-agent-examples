include("${CMAKE_CURRENT_LIST_DIR}/../../paths.cmake")
idf_component_register(SRCS "${UNIFIED_SOURCE_ROOT}/application/services/media/ai_audio.c" "${UNIFIED_SOURCE_ROOT}/application/services/media/ai_playback.c"
    INCLUDE_DIRS "${UNIFIED_SOURCE_ROOT}/application/services/media/include")
