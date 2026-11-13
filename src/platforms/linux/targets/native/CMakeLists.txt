cmake_minimum_required(VERSION 3.21)
project(unified_ai_linux C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "Build this target on Linux (Pi, Linux host or cross-toolchain)")
endif()
get_filename_component(UNIFIED_APP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)
set(UNIFIED_SOURCE_ROOT "${UNIFIED_APP_ROOT}/src")
set(LINUX_BOARD "pi3_model_b" CACHE STRING "Linux board profile")
if(NOT LINUX_BOARD STREQUAL "pi3_model_b")
    message(FATAL_ERROR "Unsupported Linux board: ${LINUX_BOARD}")
endif()
include("${UNIFIED_SOURCE_ROOT}/boards/raspberry_pi/${LINUX_BOARD}/board.cmake")
set(LINUX_ROOT "${UNIFIED_SOURCE_ROOT}/platforms/linux")
set(LVGL_SOURCE_DIR "${UNIFIED_APP_ROOT}/managed_components/lvgl__lvgl" CACHE PATH "LVGL 9.5.0 source")
if(NOT EXISTS "${LVGL_SOURCE_DIR}/lv_version.h")
    message(FATAL_ERROR "Set LVGL_SOURCE_DIR to LVGL 9.5.0 sources; see platforms/linux/README.md")
endif()
file(READ "${LVGL_SOURCE_DIR}/lv_version.h" _lv_version)
foreach(_part MAJOR MINOR PATCH)
    string(REGEX MATCH "#define LVGL_VERSION_${_part} +([0-9]+)" _match "${_lv_version}")
    set(_lv_${_part} "${CMAKE_MATCH_1}")
endforeach()
if(NOT "${_lv_MAJOR}.${_lv_MINOR}.${_lv_PATCH}" STREQUAL "9.5.0")
    message(FATAL_ERROR "Use the verified LVGL 9.5.0 baseline")
endif()
set(LV_BUILD_CONF_PATH "${LINUX_ROOT}/components/ui/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${LVGL_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/lvgl")
find_package(SDL2 REQUIRED CONFIG)
add_library(linux_demo STATIC "${LINUX_ROOT}/components/demo/demo_session.c")
target_include_directories(linux_demo PUBLIC "${LINUX_ROOT}/components/demo/include"
    "${UNIFIED_SOURCE_ROOT}/application/core/include")
add_executable(unified_ai_linux
    "${LINUX_ROOT}/entry/main.c"
    "${LINUX_ROOT}/components/runtime/runtime.c"
    "${LINUX_ROOT}/components/network/network.c"
    "${LINUX_ROOT}/components/ui/ui.c"
    "${UNIFIED_SOURCE_ROOT}/application/core/ai_app.c"
    "${UNIFIED_SOURCE_ROOT}/application/core/ai_controller.c"
    "${UNIFIED_SOURCE_ROOT}/application/ui/ai_lvgl.c")
set_target_properties(unified_ai_linux PROPERTIES OUTPUT_NAME "${LINUX_BOARD_OUTPUT_NAME}")
target_compile_definitions(unified_ai_linux PRIVATE AI_UI_NO_PROVISION
    LINUX_BOARD_NAME="${LINUX_BOARD_DISPLAY_NAME}")
target_compile_options(unified_ai_linux PRIVATE -Wall -Wextra -Werror)
target_include_directories(unified_ai_linux PRIVATE
    "${UNIFIED_SOURCE_ROOT}/application/ui/include"
    "${LINUX_ROOT}/components/runtime/include"
    "${LINUX_ROOT}/components/network/include"
    "${LINUX_ROOT}/components/ui/include")
target_link_libraries(unified_ai_linux PRIVATE linux_demo lvgl SDL2::SDL2)
include(CTest)
if(BUILD_TESTING)
    add_executable(test_linux_demo "${UNIFIED_APP_ROOT}/tests/test_linux_demo_session.c"
        "${UNIFIED_SOURCE_ROOT}/application/core/ai_app.c")
    target_compile_options(test_linux_demo PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    target_link_libraries(test_linux_demo PRIVATE linux_demo)
    add_test(NAME linux_demo_session COMMAND test_linux_demo)
    add_executable(test_linux_core "${UNIFIED_APP_ROOT}/tests/test_core.c"
        "${UNIFIED_SOURCE_ROOT}/application/core/ai_app.c")
    target_include_directories(test_linux_core PRIVATE "${UNIFIED_SOURCE_ROOT}/application/core/include")
    target_compile_options(test_linux_core PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    add_test(NAME linux_core_lifecycle COMMAND test_linux_core)
    add_executable(test_linux_controller "${UNIFIED_APP_ROOT}/tests/test_controller.c"
        "${UNIFIED_SOURCE_ROOT}/application/core/ai_app.c"
        "${UNIFIED_SOURCE_ROOT}/application/core/ai_controller.c")
    target_include_directories(test_linux_controller PRIVATE "${UNIFIED_SOURCE_ROOT}/application/core/include")
    target_compile_options(test_linux_controller PRIVATE -Wall -Wextra -Werror -UNDEBUG)
    add_test(NAME linux_controller COMMAND test_linux_controller)
    add_test(NAME linux_ui_smoke COMMAND unified_ai_linux --smoke-test)
    set_tests_properties(linux_ui_smoke PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy" TIMEOUT 15)
endif()
install(TARGETS unified_ai_linux RUNTIME DESTINATION bin)
