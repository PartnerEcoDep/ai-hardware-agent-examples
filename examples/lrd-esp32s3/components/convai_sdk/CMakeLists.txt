# ConvAI SDK 组件 — ESP32-S3
# 优先从仓库 src/ 源码构建；否则回退 libs/esp32-s3/libconvai_sdk.a 预编译库；
# 两者皆无时仅链接占位实现。
# 注意：预编译库含未重命名的 cJSON_* 符号，与 espressif/cjson 组件存在
# multiple-definition 冲突，见 main/CMakeLists.txt 的 -Wl,--allow-multiple-definition。

get_filename_component(CONVAI_REPO_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../../../.." ABSOLUTE)

set(CONVAI_SDK_SOURCE_DIR "${CONVAI_REPO_ROOT}/src")
set(SDK_LIB_PATH "${CONVAI_REPO_ROOT}/libs/esp32-s3/libconvai_sdk.a")
set(SDK_INC_PATH "${CONVAI_REPO_ROOT}/include/convai")

# 占位源文件保证 ${COMPONENT_LIB} 为普通静态库，可承载下方 IMPORTED/子目录库
idf_component_register(
    SRCS "convai_sdk_placeholder.c"
    INCLUDE_DIRS "${SDK_INC_PATH}"
)

if(EXISTS "${CONVAI_SDK_SOURCE_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${CONVAI_SDK_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/convai_sdk_source"
    )

    if(NOT TARGET convai_sdk)
        message(FATAL_ERROR
            "ConvAI source build did not define the required 'convai_sdk' target: "
            "${CONVAI_SDK_SOURCE_DIR}")
    endif()

    # 安全加固（仅作用于 SDK 库本身）：
    # -fstack-protector-strong -D_FORTIFY_SOURCE=2 始终开启；-ftrapv 仅 Debug
    target_compile_options(convai_sdk PRIVATE
        -fstack-protector-strong
        -D_FORTIFY_SOURCE=2
        $<$<CONFIG:Debug>:-ftrapv>
    )

    target_link_libraries(${COMPONENT_LIB} INTERFACE convai_sdk)
    message(STATUS "convai_sdk: building from source: ${CONVAI_SDK_SOURCE_DIR}")
elseif(EXISTS "${SDK_LIB_PATH}")
    add_library(__convai_sdk_impl STATIC IMPORTED)
    set_target_properties(__convai_sdk_impl PROPERTIES
        IMPORTED_LOCATION "${SDK_LIB_PATH}"
    )
    set_property(TARGET __convai_sdk_impl APPEND PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES "${SDK_INC_PATH}"
    )

    target_link_libraries(${COMPONENT_LIB} INTERFACE __convai_sdk_impl)

    message(STATUS "convai_sdk: using REAL SDK at ${SDK_LIB_PATH}")
else()
    message(STATUS "convai_sdk: SDK not found (no source at ${CONVAI_SDK_SOURCE_DIR}/CMakeLists.txt, "
                   "no archive at ${SDK_LIB_PATH}); placeholder only")
endif()
