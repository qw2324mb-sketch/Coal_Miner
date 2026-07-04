// #include "lora_receiver.h"
// #include "driver/uart.h"
// #include "esp_log.h"
// #include <string.h>
// #include <stdlib.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"

// static const char *TAG = "LORA_RX";

// #define LORA_UART_NUM   UART_NUM_2
// #define LORA_TX_PIN     17   // ESP32-S3 TX -> LoRa RX（可不接）
// #define LORA_RX_PIN     16   // ESP32-S3 RX <- LoRa TX
// #define LORA_BAUD_RATE  9600
// #define RX_BUF_SIZE     512

// // 解析 JSON 字符串，提取 t, h, d, p, m
// static void parse_sensor_data(const char *json_str) {
//     float temp = 0, hum = 0, dist = 0, pm25 = 0, methane = 0;
//     const char *p = json_str;

//     while (*p) {
//         if (*p == '"' && *(p+1) == 't' && *(p+2) == '"' && *(p+3) == ':') {
//             temp = strtof(p+4, NULL);
//         } else if (*p == '"' && *(p+1) == 'h' && *(p+2) == '"' && *(p+3) == ':') {
//             hum = strtof(p+4, NULL);
//         } else if (*p == '"' && *(p+1) == 'd' && *(p+2) == '"' && *(p+3) == ':') {
//             dist = strtof(p+4, NULL);
//         } else if (*p == '"' && *(p+1) == 'p' && *(p+2) == '"' && *(p+3) == ':') {
//             pm25 = strtof(p+4, NULL);
//         } else if (*p == '"' && *(p+1) == 'm' && *(p+2) == '"' && *(p+3) == ':') {
//             methane = strtof(p+4, NULL);
//         }
//         p++;
//     }

//     ESP_LOGI(TAG, "========== 接收传感器数据 ==========");
//     ESP_LOGI(TAG, "温度: %.2f °C", temp);
//     ESP_LOGI(TAG, "湿度: %.2f %%RH", hum);
//     ESP_LOGI(TAG, "顶板距离: %.1f cm", dist);
//     ESP_LOGI(TAG, "PM2.5: %.1f µg/m³", pm25);
//     ESP_LOGI(TAG, "甲烷浓度: %.1f %%LEL", methane);
//     ESP_LOGI(TAG, "===================================");
// }

// // 接收任务：逐字节读取，按行解析
// static void lora_receive_task(void *pvParameters) {
//     char rx_buffer[RX_BUF_SIZE];
//     int buffer_index = 0;
//     uint8_t byte;

//     while (1) {
//         int len = uart_read_bytes(LORA_UART_NUM, &byte, 1, pdMS_TO_TICKS(1000));
//         if (len == 1) {
//             // 存储到缓冲区
//             if (buffer_index < RX_BUF_SIZE - 1) {
//                 rx_buffer[buffer_index++] = byte;
//             } else {
//                 buffer_index = 0;
//                 ESP_LOGW(TAG, "缓冲区溢出，重置");
//             }

//             // 检测到换行符 \n，认为一帧结束
//             if (byte == '\n') {
//                 rx_buffer[buffer_index] = '\0';
//                 // 去掉末尾可能的 \r
//                 if (buffer_index >= 1 && rx_buffer[buffer_index-2] == '\r') {
//                     rx_buffer[buffer_index-2] = '\0';
//                 }
//                 if (strlen(rx_buffer) > 10) {
//                     parse_sensor_data(rx_buffer);
//                 }
//                 buffer_index = 0;   // 准备下一帧
//             }
//         } else {
//             // 超时，清空半包残留（可选）
//             if (buffer_index > 0) {
//                 buffer_index = 0;
//             }
//         }
//     }
// }

// // 初始化 UART（不发送任何 AT 命令）
// esp_err_t lora_receiver_init(void) {
//     uart_config_t uart_config = {
//         .baud_rate = LORA_BAUD_RATE,
//         .data_bits = UART_DATA_8_BITS,
//         .parity = UART_PARITY_DISABLE,
//         .stop_bits = UART_STOP_BITS_1,
//         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
//     };
//     esp_err_t err = uart_param_config(LORA_UART_NUM, &uart_config);
//     if (err != ESP_OK) return err;
//     err = uart_set_pin(LORA_UART_NUM, LORA_TX_PIN, LORA_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
//     if (err != ESP_OK) return err;
//     err = uart_driver_install(LORA_UART_NUM, RX_BUF_SIZE, 0, 0, NULL, 0);
//     if (err != ESP_OK) return err;
//     ESP_LOGI(TAG, "UART 初始化完成 (TX=GPIO%d, RX=GPIO%d, 波特率=%d)",
//              LORA_TX_PIN, LORA_RX_PIN, LORA_BAUD_RATE);
//     return ESP_OK;
// }

// void lora_receiver_start_task(void) {
//     xTaskCreate(lora_receive_task, "lora_rx", 4096, NULL, 5, NULL);
// }

#include "lora_receiver.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static lora_data_callback_t user_callback = NULL;//静态函数指针变量

static int uart_timeout_cnt = 0;

static const char *TAG = "LORA_RX";

#define LORA_UART_NUM   UART_NUM_2
#define LORA_TX_PIN     17   // ESP32-S3 TX -> LoRa RX（可不接）
#define LORA_RX_PIN     16   // ESP32-S3 RX <- LoRa TX
#define LORA_BAUD_RATE  9600
#define RX_BUF_SIZE     512
#define UART_TIMEOUT_COUNT 10  // 连续超时阈值


// 解析 JSON 字符串，提取 t, h, d, p, m
static void parse_sensor_data(const char *json_str) {
    float temp = 0, hum = 0, dist = 0, pm25 = 0, methane = 0;
    const char *p = json_str;

    while (*p) {
        if (*p == '"' && *(p+1) == 't' && *(p+2) == '"' && *(p+3) == ':') {
            temp = strtof(p+4, NULL);
        } else if (*p == '"' && *(p+1) == 'h' && *(p+2) == '"' && *(p+3) == ':') {
            hum = strtof(p+4, NULL);
        } else if (*p == '"' && *(p+1) == 'd' && *(p+2) == '"' && *(p+3) == ':') {
            dist = strtof(p+4, NULL);
        } else if (*p == '"' && *(p+1) == 'p' && *(p+2) == '"' && *(p+3) == ':') {
            pm25 = strtof(p+4, NULL);
        } else if (*p == '"' && *(p+1) == 'm' && *(p+2) == '"' && *(p+3) == ':') {
            methane = strtof(p+4, NULL);
        }
        p++;
    }

    ESP_LOGI(TAG, "========== 接收传感器数据 ==========");
    ESP_LOGI(TAG, "温度: %.2f °C", temp);
    ESP_LOGI(TAG, "湿度: %.2f %%RH", hum);
    ESP_LOGI(TAG, "顶板距离: %.1f cm", dist);
    ESP_LOGI(TAG, "PM2.5: %.1f µg/m³", pm25);
    ESP_LOGI(TAG, "甲烷浓度: %.1f %%LEL", methane);
    ESP_LOGI(TAG, "===================================");
    if (user_callback) {
        user_callback(json_str);
    }

}

// 接收任务：逐字节读取，按行解析
static void lora_receive_task(void *pvParameters) {
    char rx_buffer[RX_BUF_SIZE];
    int buffer_index = 0;
    uint8_t byte;

    while (1) {
        int len = uart_read_bytes(LORA_UART_NUM, &byte, 1, pdMS_TO_TICKS(1000));
        if (len == 1) {
            //ESP_LOGI(TAG, "RX byte: 0x%02X", byte);//// 新增：打印每个收到的字节
            // 存储到缓冲区
            if (buffer_index < RX_BUF_SIZE - 1) {
                rx_buffer[buffer_index++] = byte;
            } else {
                buffer_index = 0;
                ESP_LOGW(TAG, "缓冲区溢出，重置");
            }

            // 检测到换行符 \n，认为一帧结束
            if (byte == '\n') {
                rx_buffer[buffer_index] = '\0';
                // 去掉末尾可能的 \r
                if (buffer_index >= 1 && rx_buffer[buffer_index-2] == '\r') {
                    rx_buffer[buffer_index-2] = '\0';
                }
                if (strlen(rx_buffer) > 10) {
                    parse_sensor_data(rx_buffer);
                }
                buffer_index = 0;   // 准备下一帧
            }
        } else {
            uart_timeout_cnt++;
            if (uart_timeout_cnt >= UART_TIMEOUT_COUNT) {
                ESP_LOGE(TAG, "UART 连续超时，尝试重新初始化");
                // 重新初始化 UART
                uart_driver_delete(LORA_UART_NUM);
                lora_receiver_init();  // 重新初始化
                uart_timeout_cnt = 0;
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            
        }
    }
}

// 初始化 UART（不发送任何 AT 命令）
esp_err_t lora_receiver_init(void) {
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
    err = uart_driver_install(LORA_UART_NUM, RX_BUF_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK) return err;
    ESP_LOGI(TAG, "UART 初始化完成 (TX=GPIO%d, RX=GPIO%d, 波特率=%d)",
             LORA_TX_PIN, LORA_RX_PIN, LORA_BAUD_RATE);
    return ESP_OK;
}

void lora_receiver_start_task(void) {
    xTaskCreate(lora_receive_task, "lora_rx", 8192, NULL, 5, NULL);
}
//实现注册函数
void lora_receiver_register_callback(lora_data_callback_t cb) {
    user_callback = cb;
}
