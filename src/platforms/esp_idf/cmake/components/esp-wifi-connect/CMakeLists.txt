include("${CMAKE_CURRENT_LIST_DIR}/../../paths.cmake")
set(wifi "${UNIFIED_SOURCE_ROOT}/platforms/esp_idf/components/network/esp-wifi-connect")
message("78/esp_wifi_connect: IDF_VERSION_MAJOR is ${IDF_VERSION_MAJOR}")

if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    # v6
    message("78/esp_wifi_connect: idf_component_register for v6 used")

    idf_component_register(
        SRCS
            "${wifi}/wifi_manager.cc"
            "${wifi}/wifi_configuration_ap.cc"
            "${wifi}/wifi_station.cc"
            "${wifi}/ssid_manager.cc"
            "${wifi}/esp_wifi_store.c"
            "${wifi}/dns_server.cc"
        INCLUDE_DIRS
            "${wifi}/include"
        EMBED_TXTFILES
            "${UNIFIED_SOURCE_ROOT}/application/ui/web/wifi_configuration.html"
            "${UNIFIED_SOURCE_ROOT}/application/ui/web/wifi_configuration_done.html"
        REQUIRES "ai_network" "esp_timer" "esp_http_server" "esp_wifi" "nvs_flash" "cjson"
    )
else()
    # v5
    message("78/esp_wifi_connect: idf_component_register for v5 used")

    idf_component_register(
        SRCS
            "${wifi}/wifi_manager.cc"
            "${wifi}/wifi_configuration_ap.cc"
            "${wifi}/wifi_station.cc"
            "${wifi}/ssid_manager.cc"
            "${wifi}/esp_wifi_store.c"
            "${wifi}/dns_server.cc"
        INCLUDE_DIRS
            "${wifi}/include"
        EMBED_TXTFILES
            "${UNIFIED_SOURCE_ROOT}/application/ui/web/wifi_configuration.html"
            "${UNIFIED_SOURCE_ROOT}/application/ui/web/wifi_configuration_done.html"
        REQUIRES "ai_network" "esp_timer" "esp_http_server" "esp_wifi" "nvs_flash" "json"
    )
endif()

