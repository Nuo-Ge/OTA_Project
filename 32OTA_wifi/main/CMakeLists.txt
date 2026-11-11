idf_component_register(
    SRCS
        "main.c"
        "cloud_mqtt.c"
        "protocol/ota_protocol.c"
        "transport/ota_transport.c"
        "source/ota_source.c"
        "source/ota_source_http.c"
        "task/ota_task.c"
        "wifi/wifi_manager.c"
    INCLUDE_DIRS
        "."
        "protocol"
        "transport"
        "source"
        "task"
        "wifi"
    REQUIRES
        mqtt
        mbedtls
        esp_wifi
        nvs_flash
        esp_timer
        esp_http_client
        esp_driver_gpio
        esp_driver_uart
        network_provisioning
        protocomm
        bt
)

target_add_binary_data(${COMPONENT_LIB} "../firmware/App.bin" BINARY)
target_compile_definitions(${COMPONENT_LIB} PRIVATE "-DOTA_HAS_FLASH_FIRMWARE=1")