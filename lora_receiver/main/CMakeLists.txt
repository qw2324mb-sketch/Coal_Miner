idf_component_register(
    SRCS "main.c" "lora_receiver.c" "wifi_mqtt.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_uart esp_wifi mqtt nvs_flash esp_event
)