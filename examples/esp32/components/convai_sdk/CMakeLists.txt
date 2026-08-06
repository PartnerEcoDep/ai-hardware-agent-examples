# ConvAI SDK — ESP32-S3 预编译静态库组件
#
# 使用方法:
#   将内部编译好的 libconvai_sdk.a 放到 ../../libs/esp32s3/ 目录下,
#   并在 main/CMakeLists.txt 中将 convai_sdk_stub 改为 convai_sdk。
#
# 当前状态: 使用 STUB 模式 (convai_sdk_stub), 本组件不参与构建。

set(SDK_LIB_PATH "${CMAKE_SOURCE_DIR}/../../libs/esp32s3/libconvai_sdk.a")

if(EXISTS "${SDK_LIB_PATH}")
    # 正式 SDK 存在 → 创建 IMPORTED 库
    add_library(convai_sdk STATIC IMPORTED)
    set_target_properties(convai_sdk PROPERTIES
        IMPORTED_LOCATION "${SDK_LIB_PATH}"
    )
    set_property(TARGET convai_sdk APPEND PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES
        "${CMAKE_SOURCE_DIR}/../../include/convai"
    )
    message(STATUS "convai_sdk: using REAL SDK at ${SDK_LIB_PATH}")
else()
    # SDK 未就绪 → 返回空组件 (不阻塞构建, 由 convai_sdk_stub 代为提供符号)
    message(STATUS "convai_sdk: REAL SDK not found — build uses convai_sdk_stub instead")
endif()

idf_component_register()
