include("${CMAKE_CURRENT_LIST_DIR}/../../paths.cmake")
idf_build_get_property(UNIFIED_BOARD UNIFIED_BOARD)
set(board "${UNIFIED_SOURCE_ROOT}/boards/esp32/${UNIFIED_BOARD}")
set(common "${UNIFIED_SOURCE_ROOT}/boards/esp32/common/esp32s3")
if(UNIFIED_BOARD STREQUAL "szpi")
    set(expander "${board}/pca9557.c")
else()
    set(expander "${board}/tpt29555a.c")
endif()
idf_component_register(SRCS "${common}/board.c" "${common}/audio_codec_lckfb.c"
    "${board}/board_init.c" "${board}/lcd_init.c" "${board}/profile.c" "${expander}"
    INCLUDE_DIRS "${UNIFIED_SOURCE_ROOT}/boards/esp32/include" "${common}/include"
    PRIV_INCLUDE_DIRS "${board}" "${common}"
    REQUIRES esp_lcd
    PRIV_REQUIRES esp_driver_gpio esp_driver_i2s esp_driver_i2c esp_driver_spi esp_codec_dev)
