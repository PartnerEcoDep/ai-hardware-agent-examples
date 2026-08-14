# ESP32-S3 AI Hardware Agent — 项目 CMakeLists.txt
#
# 构建方法 (需要安装 ESP-IDF):
#   cd examples/esp32
#   idf.py set-target esp32s3
#   idf.py build
#   idf.py -p COMx flash monitor
#
# 注意:
#   - 需要先把 libconvai_sdk.a 放到 ../../libs/esp32s3/
#   - sdkconfig.defaults 已预设 ESP32-S3 最优配置

cmake_minimum_required(VERSION 3.16)

# ESP-IDF 项目入口宏 (必须在 project() 之前)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(PROJECT_VER "1.0.0")
project(ai_hardware_agent_esp32)

# 启用组件管理器 (如需额外组件)
# list(APPEND EXTRA_COMPONENT_DIRS components)
