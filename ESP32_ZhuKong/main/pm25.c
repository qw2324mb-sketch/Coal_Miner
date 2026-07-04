// // pm25.c
// #include "pm25.h"
// #include "driver/uart.h"
// #include "esp_log.h"
// #include <string.h>

// static const char *TAG = "PM25";
// static float s_concentration = 0.0f;
// static bool s_has_new_data = false;

// // K 系数标定，一般建议 0.4，可根据实际对比调整
// static const float K_FACTOR = 0.1f;

// // 前向声明
// static void pm25_task_update(void);

// // 计算校验和：之前所有字节累加和的低 7 位
// static uint8_t calc_checksum(const uint8_t *data, int len) {
//     uint16_t sum = 0;
//     for (int i = 0; i < len; i++) {
//         sum += data[i];
//     }
//     return sum & 0x7F;   // 取低 7 位
// }

// // 解析一帧 4 字节数据
// static bool parse_frame(const uint8_t *frame, float *value) {
//     // 兼容 0xAF 和 0xA5 两种帧头
//     if (frame[0] != 0xAF && frame[0] != 0xA5) {
//         return false;
//     }
//     uint8_t checksum = calc_checksum(frame, 3);
//     if (checksum != frame[3]) {
//         ESP_LOGW(TAG, "校验和错误: 期望 0x%02X, 实际 0x%02X", checksum, frame[3]);
//         return false;
//     }
//     uint16_t raw = (frame[1] << 7) | frame[2];
//     *value = raw * K_FACTOR;
//     return true;
// }

// esp_err_t pm25_init(void) {
//     uart_config_t uart_config = {
//         .baud_rate = PM25_UART_BAUD_RATE,
//         .data_bits = UART_DATA_8_BITS,
//         .parity = UART_PARITY_DISABLE,
//         .stop_bits = UART_STOP_BITS_1,
//         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
//         .source_clk = UART_SCLK_DEFAULT,
//     };
//     esp_err_t err = uart_param_config(PM25_UART_NUM, &uart_config);
//     if (err != ESP_OK) return err;
//     err = uart_set_pin(PM25_UART_NUM, PM25_UART_TX_PIN, PM25_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
//     if (err != ESP_OK) return err;
//     err = uart_driver_install(PM25_UART_NUM, 256, 0, 0, NULL, 0);
//     if (err != ESP_OK) return err;

//     ESP_LOGI(TAG, "PM2.5 传感器初始化完成（UART%d, RX=GPIO%d）", PM25_UART_NUM + 1, PM25_UART_RX_PIN);
//     return ESP_OK;
// }

// // 从 UART 读取数据并更新最新值
// static void pm25_task_update(void) {
//     uint8_t buffer[4];
//     int len = uart_read_bytes(PM25_UART_NUM, buffer, 4, pdMS_TO_TICKS(10));
//     if (len > 0) {
//         ESP_LOGI(TAG, "Received %d bytes: %02X %02X %02X %02X", len,
//                  len>=1?buffer[0]:0, len>=2?buffer[1]:0, len>=3?buffer[2]:0, len>=4?buffer[3]:0);
//     }
//     if (len == 4) {
//         float conc;
//         if (parse_frame(buffer, &conc)) {
//             s_concentration = conc;
//             s_has_new_data = true;
//             ESP_LOGD(TAG, "新浓度: %.1f µg/m³", conc);
//         }
//     }
// }

// bool pm25_get_concentration(float *concentration) {
//     uint8_t buffer[4];
//     // 阻塞等待最多 500ms 收到 4 字节
//     int len = uart_read_bytes(PM25_UART_NUM, buffer, 4, pdMS_TO_TICKS(500));
//     if (len != 4) return false;

//     // 兼容 0xA5 和 0xAF
//     if (buffer[0] != 0xA5 && buffer[0] != 0xAF) return false;
//     uint8_t sum = (buffer[0] + buffer[1] + buffer[2]) & 0x7F;
//     if (sum != buffer[3]) return false;

//     uint16_t raw = (buffer[1] << 7) | (buffer[2] & 0x7F);
//     *concentration = raw * K_FACTOR;
//     return true;
// }
#include "pm25.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "PM25";
static float s_latest_concentration = 0.0f;
static bool s_has_new_data = false;

#define K_FACTOR 0.1f   // 系数，根据传感器手册调整

// 后台接收任务
static void pm25_receive_task(void *pvParameters) {
    uint8_t buffer[4];
    while (1) {
        int len = uart_read_bytes(PM25_UART_NUM, buffer, 4, pdMS_TO_TICKS(200));
        if (len == 4) {
            // 帧头校验 0xA5 或 0xAF
            if (buffer[0] == 0xA5 || buffer[0] == 0xAF) {
                uint8_t sum = (buffer[0] + buffer[1] + buffer[2]) & 0x7F;
                if (sum == buffer[3]) {
                    uint16_t raw = (buffer[1] << 7) | (buffer[2] & 0x7F);
                    s_latest_concentration = raw * K_FACTOR;
                    s_has_new_data = true;
                    ESP_LOGD(TAG, "PM2.5 更新: %.1f µg/m³", s_latest_concentration);
                }
            }
        }
        // 持续循环，不阻塞
    }
}

esp_err_t pm25_init(void) {
    uart_config_t uart_config = {
        .baud_rate = PM25_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t err = uart_param_config(PM25_UART_NUM, &uart_config);
    if (err != ESP_OK) return err;
    err = uart_set_pin(PM25_UART_NUM, PM25_UART_TX_PIN, PM25_UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;
    err = uart_driver_install(PM25_UART_NUM, 256, 0, 0, NULL, 0);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "PM2.5 传感器初始化完成（UART%d, RX=GPIO%d）", PM25_UART_NUM + 1, PM25_UART_RX_PIN);
    return ESP_OK;
}

void pm25_start_task(void) {
    xTaskCreate(pm25_receive_task, "pm25_rx", 2048, NULL, 3, NULL);
}

float pm25_get_latest(void) {
    return s_has_new_data ? s_latest_concentration : 0.0f;
}

// 保留原有接口，改为非阻塞版本
bool pm25_get_concentration(float *concentration) {
    if (!s_has_new_data) return false;
    *concentration = s_latest_concentration;
    return true;
}