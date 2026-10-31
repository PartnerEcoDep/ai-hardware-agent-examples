cmake_minimum_required(VERSION 3.21)
if(DEFINED UNIFIED_TARGET AND NOT UNIFIED_TARGET STREQUAL "esp32s3")
    message(FATAL_ERROR "This entry builds esp32s3 only")
endif()
set(UNIFIED_TARGET esp32s3 CACHE STRING "Build target")
include("${CMAKE_CURRENT_LIST_DIR}/../../../../../CMakeLists.txt")
# A literal project call prevents CMake's implicit native project before IDF setup.
# The included dispatcher has already initialized the actual project.
if(NOT CMAKE_PROJECT_NAME)
    project(unified_ai_app NONE)
endif()
