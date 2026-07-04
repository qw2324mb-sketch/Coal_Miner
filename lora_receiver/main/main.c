// #include <stdio.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "esp_log.h"
// #include "lora_receiver.h"

// static const char *TAG = "MAIN";

// void app_main(void) {
//     ESP_LOGI(TAG, "2启动 LoRa 接收端 (ESP32-S3)");

//     // 初始化 UART（不配置 LoRa 模块，假设模块已就绪）
//     if (lora_receiver_init() != ESP_OK) {
//         ESP_LOGE(TAG, "UART 初始化失败，重启...");
//         esp_restart();
//     }

//     // 启动接收任务
//     lora_receiver_start_task();

//     // 主任务空闲循环
//     while (1) {
//         vTaskDelay(pdMS_TO_TICKS(10000));
//         ESP_LOGI(TAG, "接收端运行中...");
//     }
// }
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>              // 添加时间头文件
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "lora_receiver.h"
#include "wifi_mqtt.h"

static const char *TAG = "MAIN";
static QueueHandle_t data_queue = NULL;

// 监控用时间戳
static time_t last_lora_activity = 0;
static time_t last_mqtt_activity = 0;

static void mqtt_publish_task(void *pvParameters) {
    char *json_str;
    while (1) {
        if (xQueueReceive(data_queue, &json_str, portMAX_DELAY)) {
            // 发布并更新时间戳
            if (wifi_mqtt_publish("Worker/post", json_str) == ESP_OK) {
                last_mqtt_activity = time(NULL);
                ESP_LOGI(TAG, "MQTT 发布成功");
            } else {
                ESP_LOGE(TAG, "MQTT 发布失败");
            }
            free(json_str);
        }
    }
}

static void lora_data_callback(const char *json_str) {
    // 收到 LoRa 数据时更新时间戳
    last_lora_activity = time(NULL);
    
    char *copy = strdup(json_str);
    if (copy) {
        if (xQueueSend(data_queue, &copy, pdMS_TO_TICKS(10)) != pdTRUE) {
            free(copy);
            ESP_LOGW(TAG, "队列满，丢弃数据");
        }
    }
}

// 看门狗任务：监控两个任务的活动
static void watchdog_task(void *pvParameters) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000)); // 每分钟检查一次
        time_t now = time(NULL);
        if (last_lora_activity != 0 && (now - last_lora_activity) > 300) {
            ESP_LOGE(TAG, "LoRa 任务无数据超过5分钟，重启系统");
            esp_restart();
        }
        if (last_mqtt_activity != 0 && (now - last_mqtt_activity) > 300) {
            ESP_LOGE(TAG, "MQTT 任务无发布超过5分钟，重启系统");
            esp_restart();
        }
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "启动 LoRa 接收 + MQTT 上传");

    data_queue = xQueueCreate(10, sizeof(char*));
    if (!data_queue) {
        ESP_LOGE(TAG, "创建队列失败");
        return;
    }

    if (lora_receiver_init() != ESP_OK) {
        ESP_LOGE(TAG, "LoRa 初始化失败");
        return;
    }
    lora_receiver_register_callback(lora_data_callback);
    lora_receiver_start_task();

    if (wifi_mqtt_init() != ESP_OK) {
        ESP_LOGE(TAG, "WiFi/MQTT 初始化失败");
        return;
    }

    // 增加堆栈大小防止溢出
    xTaskCreate(mqtt_publish_task, "mqtt_pub", 8192, NULL, 3, NULL);
    xTaskCreate(watchdog_task, "watchdog", 4096, NULL, 1, NULL);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "系统运行中...");
    }
}