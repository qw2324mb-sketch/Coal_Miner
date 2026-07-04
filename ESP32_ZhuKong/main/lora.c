#include "lora.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "LORA";

// 硬件配置（请根据实际接线修改）
#define LORA_UART_NUM     UART_NUM_2
#define LORA_TX_PIN       17          // ESP32 发送 -> LoRa RX
#define LORA_RX_PIN       16          // ESP32 接收 <- LoRa TX
#define LORA_BAUD_RATE    9600

/**
 * @brief 初始化 UART，不发送任何 AT 指令
 */
esp_err_t lora_init(void) {
    uart_config_t uart_config = {
        .baud_rate = LORA_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };
    esp_err_t err = uart_param_config(LORA_UART_NUM, &uart_config);
    if (err != ESP_OK) return err;
    err = uart_set_pin(LORA_UART_NUM, LORA_TX_PIN, LORA_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;
    err = uart_driver_install(LORA_UART_NUM, 256, 0, 0, NULL, 0);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "UART 初始化完成 (TX=GPIO%d, RX=GPIO%d, 波特率=%d)",
             LORA_TX_PIN, LORA_RX_PIN, LORA_BAUD_RATE);
    return ESP_OK;
}

bool lora_send_data(const uint8_t *data, size_t len) {
    int written = uart_write_bytes(LORA_UART_NUM, (const char *)data, len);
    return (written == len);
}

bool lora_send_string(const char *str) {
    return lora_send_data((const uint8_t *)str, strlen(str));
}