# ConvAI SDK Stub — 桩组件 (正式 SDK 到位后替换)
#
# 本组件提供所有 SDK API 符号的 mock 实现。
# 换正式 SDK: 从 main/CMakeLists.txt 中把 convai_sdk_stub 改为 convai_sdk。

idf_component_register(
    SRCS
        "convai_sdk_stub.c"
    INCLUDE_DIRS
        "${CMAKE_SOURCE_DIR}/../../include/convai"
)
