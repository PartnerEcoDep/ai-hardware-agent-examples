# ConvAI SDK — ESP32-S3 预编译静态库组件
#
# 将内部编译好的 libconvai_sdk.a 放到 ../../libs/esp32s3/ 目录下，
# 并在 main/CMakeLists.txt 中 REQUIRES 里引用 convai_sdk。
#
# 注意：SDK 库内置了未重命名的 cJSON_* 符号，与 espressif/cjson 组件存在
# 潜在 multiple-definition 冲突；该问题由使用方在构建时自行处理。

set(SDK_LIB_PATH "${CMAKE_SOURCE_DIR}/../../libs/esp32s3/libconvai_sdk.a")
set(SDK_INC_PATH "${CMAKE_SOURCE_DIR}/../../include/convai")

# 1) 先登记组件（含 include 路径 + 占位源文件，保证 ${COMPONENT_LIB} 是普通
#    静态库而非 INTERFACE 库，后续才能用 INTERFACE 方式承载 IMPORTED 库）
idf_component_register(
    SRCS "convai_sdk_placeholder.c"
    INCLUDE_DIRS "${SDK_INC_PATH}"
)

if(EXISTS "${SDK_LIB_PATH}")
    # 2) 再把预编译静态库作为 IMPORTED 库导入
    add_library(__convai_sdk_impl STATIC IMPORTED)
    set_target_properties(__convai_sdk_impl PROPERTIES
        IMPORTED_LOCATION "${SDK_LIB_PATH}"
    )
    set_property(TARGET __convai_sdk_impl APPEND PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES "${SDK_INC_PATH}"
    )

    # 3) 用 INTERFACE 关键字把导入库链接进组件，依赖本组件者即获得 SDK 符号
    target_link_libraries(${COMPONENT_LIB} INTERFACE __convai_sdk_impl)

    message(STATUS "convai_sdk: using REAL SDK at ${SDK_LIB_PATH}")
else()
    message(STATUS "convai_sdk: REAL SDK not found at ${SDK_LIB_PATH}")
endif()
