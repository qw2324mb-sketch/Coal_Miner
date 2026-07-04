// #include <stdio.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/queue.h"
// #include "freertos/semphr.h"
// #include "esp_timer.h"
// #include "esp_log.h"
// #include "sht30.h"
// #include "ultrasonic.h"
// #include "pm25.h"
// #include "alarm.h"
// #include "methane.h"
// #include <math.h>

// static const char *TAG = "MAIN";

// // 定义数据类型枚举
// typedef enum {
//     DATA_TYPE_SHT30,
//     DATA_TYPE_ULTRASONIC,
//     DATA_TYPE_PM25,
//     DATA_TYPE_METHANE 
// } data_type_t;

// // 数据队列消息结构
// typedef struct {
//     data_type_t type;
//     float value1;      // 温度 或 距离
//     float value2;      // 湿度（仅SHT30有效）
//     uint32_t timestamp_ms;
// } sensor_data_t;

// // 队列句柄
// static QueueHandle_t xDataQueue = NULL;

// // I2C 互斥锁（防止 SHT30 任务与超声波任务中的温度读取冲突）
// static SemaphoreHandle_t xI2CMutex = NULL;

// // ------------------- 任务函数 -------------------

// // 任务：温湿度传感器（主动周期性读取）
// static void task_sht30(void *pvParameters) {
//     float temp = 0, hum = 0;
//     TickType_t xLastWakeTime = xTaskGetTickCount();
//     while (1) {
//         // 获取 I2C 互斥锁
//         if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
//             esp_err_t ret = sht30_read_measurement(&temp, &hum);
//             xSemaphoreGive(xI2CMutex);
//             if (ret == ESP_OK) {
//                 sensor_data_t data = {
//                     .type = DATA_TYPE_SHT30,
//                     .value1 = temp,
//                     .value2 = hum,
//                     .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000)
//                 };
//                 xQueueSend(xDataQueue, &data, 0);
//                 ESP_LOGD("SHT30", "温度=%.2f, 湿度=%.2f", temp, hum);
//             } else {
//                 ESP_LOGW("SHT30", "读取失败");
//             }
//         } else {
//             ESP_LOGW("SHT30", "获取 I2C 锁超时");
//         }
//         vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));  // 2秒周期
//     }
// }

// // 任务：超声波测距（每次独立读取温湿度用于补偿，使用 I2C 锁）
// static void task_ultrasonic(void *pvParameters) {
//     float distance = 0;
//     float temperature = 25.0f;
//     static float last_valid_distance = -1;      // 上一次有效距离
//     static int64_t last_valid_time_us = 0;      // 上一次有效时间戳
//     static int discard_count = 0;               // 丢弃计数器
//     const int DISCARD_BEFORE_STABLE = 5;        // 丢弃前5次测量值
//     TickType_t xLastWakeTime = xTaskGetTickCount();

//     while (1) {
//         // 获取当前温度
//         if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
//             sht30_read_measurement(&temperature, NULL);
//             xSemaphoreGive(xI2CMutex);
//         }

//         bool ok = ultrasonic_read_cm(&distance, temperature);
//         if (ok && distance >= 23.0f) {
//             // ----- 预热丢弃 -----
//             if (discard_count < DISCARD_BEFORE_STABLE) {
//                 discard_count++;
//                 ESP_LOGW("Ultrasonic", "预热丢弃第 %d 次测量: %.1f cm", discard_count, distance);
//                 // 仍然将数据发送到队列以便调试（可选）
//                 sensor_data_t data = {
//                     .type = DATA_TYPE_ULTRASONIC,
//                     .value1 = distance,
//                     .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000)
//                 };
//                 xQueueSend(xDataQueue, &data, 0);
//                 vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));
//                 continue;   // 跳过后续处理，不更新 last_valid_distance
//             }
//             // ----- 变化率滤波（防止跳变）-----
//             int64_t now_us = esp_timer_get_time();
//             if (last_valid_distance > 0 && last_valid_time_us > 0) {
//                 float delta_d = distance - last_valid_distance;
//                 float dt = (now_us - last_valid_time_us) / 1e6f;
//                 float rate = delta_d / dt;   // 单位：cm/s
//                 // 若单次变化超过 30cm 或 速率 > 20cm/s，视为异常跳变，保持上次值
//                 if (fabs(delta_d) > 30.0f || fabs(rate) > 20.0f) {
//                     ESP_LOGW("Ultrasonic", "异常跳变: %.1f -> %.1f (%.1f cm/s), 使用上次值 %.1f",
//                              last_valid_distance, distance, rate, last_valid_distance);
//                     distance = last_valid_distance;
//                 } else {
//                     last_valid_distance = distance;
//                     last_valid_time_us = now_us;
//                 }
//             } else {
//                 // 首次有效测量
//                 last_valid_distance = distance;
//                 last_valid_time_us = esp_timer_get_time();
//             }

//             // ----- 顶板坍塌风险报警 -----
//             #define DANGER_DISTANCE  50.0f   // 危险距离阈值 (cm)
//             #define RAPID_DROP_RATE  15.0f   // 快速下沉速率阈值 (cm/s)

//             bool danger = false;
//             if (distance < DANGER_DISTANCE) {
//                 ESP_LOGW("ALERT", "⚠️ 顶板距离过近: %.1f cm !!!", distance);
//                 danger = true;
//             } else if (last_valid_time_us > 0) {
//                 int64_t now = esp_timer_get_time();
//                 float dt = (now - last_valid_time_us) / 1e6f;
//                 if (dt > 0.1f && dt < 5.0f) {  // 避免初始或间隔过长误判
//                     float rate = (last_valid_distance - distance) / dt;  // 正值表示下降
//                     if (rate > RAPID_DROP_RATE) {
//                         ESP_LOGW("ALERT", "⚠️ 顶板快速下沉: %.1f cm/s !!!", rate);
//                         danger = true;
//                     }
//                 }
//             }

//             // 可以驱动 GPIO 输出报警（例如点亮 LED 或蜂鸣器）
//             // 调用报警模块设置 GPIO 状态
//             alarm_set(ALARM_TYPE_ROOF, danger);

//             // 发送最终距离到显示队列
//             sensor_data_t data = {
//                 .type = DATA_TYPE_ULTRASONIC,
//                 .value1 = distance,
//                 .timestamp_ms = (uint32_t)(now_us / 1000)
//             };
//             xQueueSend(xDataQueue, &data, 0);
//             ESP_LOGD("Ultrasonic", "距离=%.1f cm (温度=%.1f)", distance, temperature);
//         } else {
//             ESP_LOGW("Ultrasonic", "测距失败");
//         }
//         vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));
//     }
// }

// // 任务：粉尘传感器（阻塞等待 UART 数据）
// static void task_pm25(void *pvParameters) {
//     float concentration = 0;
//     while (1) {
//         // pm25_get_concentration 内部阻塞直到收到完整帧
//         if (pm25_get_concentration(&concentration)) {
//             sensor_data_t data = {
//                 .type = DATA_TYPE_PM25,
//                 .value1 = concentration,
//                 .value2 = 0,
//                 .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000)
//             };
//             xQueueSend(xDataQueue, &data, 0);
//             ESP_LOGD("PM25", "浓度=%.1f ug/m³", concentration);
//         } else {
//             ESP_LOGW("PM25", "读取超时");
//         }
//         // 传感器约 1 秒自动发一次，这里不额外延时，任务会阻塞在 uart_read_bytes
//     }
// }

// // 任务：甲烷传感器（周期性读取）
// static void task_methane(void *pvParameters) {
//     float concentration = 0;
//     TickType_t xLastWakeTime = xTaskGetTickCount();
//     const float METHANE_ALERT_THRESHOLD = 20.0f;   // 20%LEL 触发报警

//     while (1) {
//         if (methane_read(&concentration)) {
//             // 发送到显示队列
//             sensor_data_t data = {
//                 .type = DATA_TYPE_METHANE,
//                 .value1 = concentration,
//                 .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000)
//             };
//             xQueueSend(xDataQueue, &data, 0);
//             ESP_LOGD("METHANE", "浓度=%.1f %%LEL", concentration);

//             // 报警联动：浓度超限则触发报警
//             if (concentration >= METHANE_ALERT_THRESHOLD) {
//                 ESP_LOGW("ALERT", "甲烷浓度超限: %.1f %%LEL", concentration);
//                 alarm_set(ALARM_TYPE_METHANE, true);
//             } else {
//                 alarm_set(ALARM_TYPE_METHANE, false);
//             }
//         } else {
//             ESP_LOGW("METHANE", "读取失败");
//         }
//         vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));   // 每2秒一次
//     }
// }

// // 任务：汇总显示（从队列接收并打印）
// static void task_display(void *pvParameters) {
//     sensor_data_t data;
//     while (1) {
//         if (xQueueReceive(xDataQueue, &data, portMAX_DELAY)) {
//             switch (data.type) {
//                 case DATA_TYPE_SHT30:
//                     ESP_LOGI(TAG, "[SHT30] 温度: %.2f °C, 湿度: %.2f %%RH", data.value1, data.value2);
//                     break;
//                 case DATA_TYPE_ULTRASONIC:
//                     ESP_LOGI(TAG, "[Ultrasonic] 距离: %.1f cm", data.value1);
//                     break;
//                 case DATA_TYPE_PM25:
//                     ESP_LOGI(TAG, "[PM2.5] 浓度: %.1f µg/m³", data.value1);
//                     break;
//                 case DATA_TYPE_METHANE:
//                     ESP_LOGI(TAG, "[Methane] 浓度: %.1f %%LEL", data.value1);
//                     break;
//                 default:
//                     break;
//             }
//         }
//     }
// }

// // ------------------- 主函数 -------------------
// void app_main(void) {
//     ESP_LOGI(TAG, "启动多任务传感器采集系统");

//     // 1. 创建队列（最多缓存 10 条消息）
//     xDataQueue = xQueueCreate(10, sizeof(sensor_data_t));
//     // 2. 创建 I2C 互斥锁
//     xI2CMutex = xSemaphoreCreateMutex();

//     // 3. 初始化硬件驱动（注意：I2C 初始化只需一次，但多个任务会共用 I2C 总线）
//     ESP_ERROR_CHECK(sht30_init());        // 初始化 I2C 引脚
//     ESP_ERROR_CHECK(ultrasonic_init());   // 初始化 GPIO
//     ESP_ERROR_CHECK(pm25_init());         // 初始化 UART
//     ESP_ERROR_CHECK(alarm_init());      // 使用 GPIO48 作为报警输出
//     ESP_ERROR_CHECK(methane_init());      // 初始化甲烷模块
//     // 4. 创建任务
//     xTaskCreate(task_sht30, "sht30", 4096, NULL, 2, NULL);
//     xTaskCreate(task_ultrasonic, "ultrasonic", 4096, NULL, 2, NULL);
//     xTaskCreate(task_pm25, "pm25", 4096, NULL, 2, NULL);
//     xTaskCreate(task_display, "display", 4096, NULL, 1, NULL);
//     xTaskCreate(task_methane, "methane", 4096, NULL, 2, NULL);
//     // 主任务可以删除自己或做其他事
//     vTaskDelete(NULL);
// }
#include <stdio.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "sht30.h"
#include "ultrasonic.h"
#include "pm25.h"
#include "alarm.h"
//#include "methane.h"
#include "lora.h"
#include "mq4.h"

static const char *TAG = "MAIN";

// 数据类型枚举
typedef enum {
    DATA_TYPE_SHT30,
    DATA_TYPE_ULTRASONIC,
    DATA_TYPE_PM25,
    DATA_TYPE_METHANE
} data_type_t;

// 队列消息结构
typedef struct {
    data_type_t type;
    float value1;           // 温度 / 距离 / PM2.5 / 甲烷
    float value2;           // 湿度（仅 SHT30）
    uint32_t timestamp_ms;
} sensor_data_t;

static QueueHandle_t xDataQueue = NULL;
static SemaphoreHandle_t xI2CMutex = NULL;      // 保护 I2C
static SemaphoreHandle_t xSensorMutex = NULL;   // 保护共享数据（用于 LoRa）

// 共享传感器数据
static float g_temperature = 0.0f;
static float g_humidity = 0.0f;
static float g_distance = 0.0f;
static float g_pm25 = 0.0f;
static float g_methane = 0.0f;

// ------------------- 任务函数 -------------------

// 温湿度任务
static void task_sht30(void *pvParameters) {
    float temp = 0, hum = 0;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1) {
        if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            esp_err_t ret = sht30_read_measurement(&temp, &hum);
            xSemaphoreGive(xI2CMutex);
            if (ret == ESP_OK) {
                sensor_data_t data = {
                    .type = DATA_TYPE_SHT30,
                    .value1 = temp,
                    .value2 = hum,
                    .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000)
                };
                if (xQueueSendToBack(xDataQueue, &data, pdMS_TO_TICKS(10)) != pdTRUE) {
                    ESP_LOGW("SHT30", "队列满，丢弃温湿度数据");
                }
                if (xSemaphoreTake(xSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    g_temperature = temp;
                    g_humidity = hum;
                    xSemaphoreGive(xSensorMutex);
                }
                ESP_LOGD("SHT30", "温度=%.2f, 湿度=%.2f", temp, hum);
            } else {
                ESP_LOGW("SHT30", "读取失败");
            }
        } else {
            ESP_LOGW("SHT30", "获取 I2C 锁超时");
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));
    }
}

// 超声波测距任务
static void task_ultrasonic(void *pvParameters) {
    static uint8_t consecutive_bad = 0;   // 连续异常次数
    const uint8_t MAX_CONSECUTIVE_BAD = 2; // 允许的最大连续异常次数，超过则接受本次测量
    float distance = 0;
    float temperature = 25.0f;
    static float last_valid_distance = -1;
    static int64_t last_valid_time_us = 0;
    static int discard_count = 0;
    const int DISCARD_BEFORE_STABLE = 5;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    while (1) {
        if (xSemaphoreTake(xI2CMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            sht30_read_measurement(&temperature, NULL);
            xSemaphoreGive(xI2CMutex);
        }

        bool ok = ultrasonic_read_cm(&distance, temperature);
        if (ok && distance >= 23.0f) {
            // 预热丢弃
            if (discard_count < DISCARD_BEFORE_STABLE) {
                discard_count++;
                ESP_LOGW("Ultrasonic", "预热丢弃第 %d 次测量: %.1f cm", discard_count, distance);
                sensor_data_t data = {
                    .type = DATA_TYPE_ULTRASONIC,
                    .value1 = distance,
                    .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000)
                };
                xQueueSendToBack(xDataQueue, &data, pdMS_TO_TICKS(10));
                // 预热期间强制清除顶板报警，避免残留状态
                alarm_set(ALARM_TYPE_ROOF, false);
                vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));
                continue;
            }

            // 变化率滤波
            // int64_t now_us = esp_timer_get_time();
            // if (last_valid_distance > 0 && last_valid_time_us > 0) {
            //     float delta_d = distance - last_valid_distance;
            //     float dt = (now_us - last_valid_time_us) / 1e6f;
            //     float rate = delta_d / dt;
            //     if (fabs(delta_d) > 30.0f || fabs(rate) > 20.0f) {
            //         ESP_LOGW("Ultrasonic", "异常跳变: %.1f -> %.1f (%.1f cm/s), 使用上次值 %.1f",
            //                  last_valid_distance, distance, rate, last_valid_distance);
            //         distance = last_valid_distance;
            //     } else {
            //         last_valid_distance = distance;
            //         last_valid_time_us = now_us;
            //     }
            // } else {
            //     last_valid_distance = distance;
            //     last_valid_time_us = esp_timer_get_time();
            // }
            // 变化率滤波（带连续异常容忍）
            int64_t now_us = esp_timer_get_time();
            if (last_valid_distance > 0 && last_valid_time_us > 0) {
                float delta_d = distance - last_valid_distance;
                float dt = (now_us - last_valid_time_us) / 1e6f;
                float rate = delta_d / dt;
                bool is_bad = (fabs(delta_d) > 30.0f || fabs(rate) > 20.0f);

                if (is_bad) {
                    consecutive_bad++;
                    if (consecutive_bad <= MAX_CONSECUTIVE_BAD) {
                        // 容忍次数内，丢弃本次测量，使用上次值
                        ESP_LOGW("Ultrasonic", "异常跳变 (连续第%d次): %.1f -> %.1f (%.1f cm/s), 使用上次值 %.1f",
                                consecutive_bad, last_valid_distance, distance, rate, last_valid_distance);
                        distance = last_valid_distance;
                        // 注意：不更新 last_valid_distance 和 last_valid_time_us
                    } else {
                        // 连续异常超限，接受真实跳变
                        ESP_LOGW("Ultrasonic", "连续异常超限，接受真实跳变: %.1f -> %.1f (%.1f cm/s)",
                                last_valid_distance, distance, rate);
                        last_valid_distance = distance;
                        last_valid_time_us = now_us;
                        consecutive_bad = 0;
                    }
                } else {
                    // 正常测量，重置计数并更新
                    consecutive_bad = 0;
                    last_valid_distance = distance;
                    last_valid_time_us = now_us;
                }
            } else {
                // 首次测量
                last_valid_distance = distance;
                last_valid_time_us = now_us;
                consecutive_bad = 0;
            }

            // 顶板报警判断
            #define DANGER_DISTANCE  50.0f
            #define RAPID_DROP_RATE  15.0f
            bool danger = false;
            if (distance < DANGER_DISTANCE) {
                ESP_LOGW("ALERT", "⚠️ 顶板距离过近: %.1f cm !!!", distance);
                danger = true;
            } else if (last_valid_time_us > 0) {
                int64_t now = esp_timer_get_time();
                float dt = (now - last_valid_time_us) / 1e6f;
                if (dt > 0.1f && dt < 5.0f) {
                    float rate = (last_valid_distance - distance) / dt;
                    if (rate > RAPID_DROP_RATE) {
                        ESP_LOGW("ALERT", "⚠️ 顶板快速下沉: %.1f cm/s !!!", rate);
                        danger = true;
                    }
                }
            }
            alarm_set(ALARM_TYPE_ROOF, danger);

            // 发送到显示队列
            sensor_data_t data = {
                .type = DATA_TYPE_ULTRASONIC,
                .value1 = distance,
                .timestamp_ms = (uint32_t)(now_us / 1000)
            };
            xQueueSendToBack(xDataQueue, &data, pdMS_TO_TICKS(10));

            // 更新共享数据
            if (xSemaphoreTake(xSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                g_distance = distance;
                xSemaphoreGive(xSensorMutex);
            }
            ESP_LOGD("Ultrasonic", "距离=%.1f cm (温度=%.1f)", distance, temperature);
        } else {
            ESP_LOGW("Ultrasonic", "测距失败");
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));
    }
}

// 粉尘传感器任务（UART 阻塞接收）
static void task_pm25(void *pvParameters) {
    float concentration = 0;
    const float PM25_ALERT_THRESHOLD = 150.0f;  // 可根据需要调整或从配置读取
    while (1) {
        if (pm25_get_concentration(&concentration)) {
            sensor_data_t data = {
                .type = DATA_TYPE_PM25,
                .value1 = concentration,
                .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000)
            };
            if (xQueueSendToBack(xDataQueue, &data, pdMS_TO_TICKS(10)) != pdTRUE) {
                ESP_LOGW("PM25", "队列满，丢弃粉尘数据");
            }
            if (xSemaphoreTake(xSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                g_pm25 = concentration;
                xSemaphoreGive(xSensorMutex);
            }
            ESP_LOGD("PM25", "浓度=%.1f ug/m³", concentration);
            bool alert = (concentration >= PM25_ALERT_THRESHOLD);
            alarm_set(ALARM_TYPE_PM25, alert);
            if (alert) {
                ESP_LOGW("PM25", "⚠️ PM2.5 浓度超标: %.1f µg/m³", concentration);
            }
        } else {
            ESP_LOGW("PM25", "读取超时");
            //vTaskDelay(pdMS_TO_TICKS(100));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));  // 适当降低轮询频率
    }
}

// 甲烷传感器任务
static void task_methane(void *pvParameters) {
    float concentration = 0;
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const float METHANE_ALERT_THRESHOLD = 20.0f;
    while (1) {
        if (mq4_read(&concentration)) {
            sensor_data_t data = {
                .type = DATA_TYPE_METHANE,
                .value1 = concentration,
                .timestamp_ms = (uint32_t)(esp_timer_get_time() / 1000)
            };
            if (xQueueSendToBack(xDataQueue, &data, pdMS_TO_TICKS(10)) != pdTRUE) {
                ESP_LOGW("METHANE", "队列满，丢弃甲烷数据");
            }
            if (xSemaphoreTake(xSensorMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                g_methane = concentration;
                xSemaphoreGive(xSensorMutex);
            }
            if (concentration >= METHANE_ALERT_THRESHOLD) {
                ESP_LOGW("ALERT", "甲烷浓度超限: %.1f %%LEL", concentration);
                alarm_set(ALARM_TYPE_METHANE, true);
            } else {
                alarm_set(ALARM_TYPE_METHANE, false);
            }
            ESP_LOGD("METHANE", "浓度=%.1f %%LEL", concentration);
        } else {
            ESP_LOGW("METHANE", "读取失败");
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(2000));
    }
}

// 显示任务（队列接收并打印）
static void task_display(void *pvParameters) {
    sensor_data_t data;
    while (1) {
        if (xQueueReceive(xDataQueue, &data, portMAX_DELAY)) {
            switch (data.type) {
                case DATA_TYPE_SHT30:
                    ESP_LOGI(TAG, "[SHT30] 温度: %.2f °C, 湿度: %.2f %%RH", data.value1, data.value2);
                    break;
                case DATA_TYPE_ULTRASONIC:
                    ESP_LOGI(TAG, "[Ultrasonic] 距离: %.1f cm", data.value1);
                    break;
                case DATA_TYPE_PM25:
                    ESP_LOGI(TAG, "[PM2.5] 浓度: %.1f µg/m³", data.value1);
                    break;
                case DATA_TYPE_METHANE:
                    ESP_LOGI(TAG, "[Methane] 浓度: %.1f %%LEL", data.value1);
                    break;
                default:
                    break;
            }
        }
    }
}

// LoRa 无线发送任务（每5秒发送一次）
static void task_lora_sender(void *pvParameters) {
    char tx_buffer[256];
    TickType_t xLastWakeTime = xTaskGetTickCount();
    while (1) {
        if (xSemaphoreTake(xSensorMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            float temp = g_temperature, hum = g_humidity;
            float dist = g_distance;
            float pm = g_pm25;
            float meth = g_methane;
            xSemaphoreGive(xSensorMutex);

            snprintf(tx_buffer, sizeof(tx_buffer),
                     "{\"t\":%.1f,\"h\":%.1f,\"d\":%.1f,\"p\":%.1f,\"m\":%.1f}\r\n",
                     temp, hum, dist, pm, meth);

            if (lora_send_string(tx_buffer)) {
                ESP_LOGI(TAG, "LoRa 发送成功: %s", tx_buffer);
            } else {
                ESP_LOGW(TAG, "LoRa 发送失败");
            }
        } else {
            ESP_LOGW(TAG, "获取传感器数据锁超时");
        }
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(5000));
    }
}

// ------------------- 主函数 -------------------
void app_main(void) {
    ESP_LOGI(TAG, "启动多任务传感器采集系统 + LoRa 无线发送");

    // 创建队列（容量 20，避免丢包）
    xDataQueue = xQueueCreate(20, sizeof(sensor_data_t));
    xI2CMutex = xSemaphoreCreateMutex();
    xSensorMutex = xSemaphoreCreateMutex();

    // 初始化硬件模块
    ESP_ERROR_CHECK(sht30_init());        // I2C (SDA=GPIO8, SCL=GPIO9)
    ESP_ERROR_CHECK(ultrasonic_init());   // Trig=GPIO21, Echo=GPIO47
    ESP_ERROR_CHECK(pm25_init());         // UART1, RX=GPIO4 (请根据实际接线修改)
    //ESP_ERROR_CHECK(methane_init());      // ADC1_CH4 (GPIO5)
    ESP_ERROR_CHECK(mq4_init());
    // 可选：校准 R0（需要先预热传感器）
    ESP_ERROR_CHECK(alarm_init());        // GPIO48(顶板), GPIO45(甲烷)
    // 启动后台接收任务
    pm25_start_task();
    //启动后台接收任务
    vTaskDelay(pdMS_TO_TICKS(120000)); // 等待 2 分钟预热（演示可缩短）
    mq4_calibrate_r0(20);
    
    // LoRa 初始化，成功后才创建发送任务
    if (lora_init() != ESP_OK) {
    ESP_LOGW(TAG, "LoRa 初始化失败，无线发送功能不可用");
    } else {
        xTaskCreate(task_lora_sender, "lora_sender", 4096, NULL, 2, NULL);
    }

    // 创建传感器和显示任务
    xTaskCreate(task_sht30,      "sht30",      4096, NULL, 2, NULL);
    xTaskCreate(task_ultrasonic, "ultrasonic", 4096, NULL, 2, NULL);
    xTaskCreate(task_pm25,       "pm25",       4096, NULL, 2, NULL);
    xTaskCreate(task_methane,    "methane",    4096, NULL, 2, NULL);
    xTaskCreate(task_display,    "display",    4096, NULL, 1, NULL);

    // 主任务删除
    vTaskDelete(NULL);
}