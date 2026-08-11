cmake_minimum_required(VERSION 3.16)

# This example intentionally uses its own ESP-IDF project instead of the
# repository's top-level CMake build, which is for GoldieOS targets.
set(IDF_TARGET "esp32s3")
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(convai_esp32s3_example)
