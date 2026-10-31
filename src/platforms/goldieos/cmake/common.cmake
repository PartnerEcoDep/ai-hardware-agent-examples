
# Limit application-owned interfaces to their direct consumers. Vendor BSP includes
# remain target-wide because its CMake builds a single executable.
function(goldieos_source_interfaces source)
    foreach(module IN LISTS ARGN)
        if(module STREQUAL "app")
            set(dir "${UNIFIED_SOURCE_ROOT}/platforms/goldieos/include")
        else()
            set(dir "${UNIFIED_SOURCE_ROOT}/platforms/goldieos/components/${module}/include")
        endif()
        # Keep forward slashes for the vendor GCC response-file parser on Windows.
        set_property(SOURCE "${source}" APPEND PROPERTY COMPILE_OPTIONS "-I${dir}")
    endforeach()
endfunction()
# Assemble the existing GoldieOS backend without imposing it on other WS63 SDKs.
foreach(component IN ITEMS config media network runtime convai ui)
    include("${CMAKE_CURRENT_LIST_DIR}/../components/${component}/component.cmake")
endforeach()

function(unified_goldieos_common target backend)
    if(NOT backend MATCHES "^(ws63|windows)$")
        message(FATAL_ERROR "Unsupported GoldieOS backend: ${backend}")
    endif()
    target_sources(${target} PRIVATE
        ${WS63_BOARD_SOURCES}
        "${UNIFIED_SOURCE_ROOT}/platforms/goldieos/entry/app.c"
        "${UNIFIED_SOURCE_ROOT}/application/core/ai_app.c"
        "${UNIFIED_SOURCE_ROOT}/application/core/ai_controller.c"
        "${UNIFIED_SOURCE_ROOT}/application/services/cloud/ai_cloud.c"
        "${UNIFIED_SOURCE_ROOT}/application/services/media/ai_audio.c"
        "${UNIFIED_SOURCE_ROOT}/application/services/media/ai_playback.c"
        "${UNIFIED_SOURCE_ROOT}/application/services/network/ai_provision.c"
    "${UNIFIED_SOURCE_ROOT}/application/services/network/ai_wifi_store.c"
    "${UNIFIED_SOURCE_ROOT}/application/services/network/ai_network_status.c"
        "${UNIFIED_SOURCE_ROOT}/application/services/network/ai_portal.c")
    target_include_directories(${target} BEFORE PRIVATE
        "${UNIFIED_SOURCE_ROOT}/boards/ws63/include" "${ws_board_dir}"
        "${sdk_include}" "${sdk_include}/convai"
        "${UNIFIED_SOURCE_ROOT}/application/core/include" "${UNIFIED_SOURCE_ROOT}/application/services/cloud/include"
        "${UNIFIED_SOURCE_ROOT}/application/services/media/include" "${UNIFIED_SOURCE_ROOT}/application/services/network/include"
        "${UNIFIED_SOURCE_ROOT}/application/ui/include" "${GOLDIEOS_ROOT}/platform")
    goldieos_component_config(${target})
    goldieos_component_media(${target})
    goldieos_component_network(${target} ${backend})
    goldieos_component_runtime(${target} ${backend})
    goldieos_component_sdk(${target} ${backend})
    goldieos_component_ui(${target})
    set(port "${UNIFIED_SOURCE_ROOT}/platforms/goldieos")
    goldieos_source_interfaces("${port}/entry/app.c" app media network config ui runtime)
    goldieos_source_interfaces("${port}/components/config/config.c" config runtime)
    goldieos_source_interfaces("${port}/components/media/media.c" media app)
    goldieos_source_interfaces("${port}/components/ui/ui.c" ui app)
    goldieos_source_interfaces("${port}/components/network/${backend}/network.c" network)
    goldieos_source_interfaces("${port}/components/runtime/${backend}/runtime.c" runtime)
    set_target_properties(${target} PROPERTIES C_STANDARD 11)
endfunction()
