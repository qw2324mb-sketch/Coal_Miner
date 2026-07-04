// ultrasonic.c
#include "ultrasonic.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "Ultrasonic";

esp_err_t ultrasonic_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TRIG_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);

    io_conf.pin_bit_mask = (1ULL << ECHO_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);

    gpio_set_level(TRIG_GPIO, 0);
    ESP_LOGI(TAG, "超声波初始化完成: Trig=GPIO%d, Echo=GPIO%d", TRIG_GPIO, ECHO_GPIO);
    return ESP_OK;
}

static uint32_t pulse_in_us(void)
{
    // 等待 Echo 变为高电平，超时返回 0
    int64_t start_time = esp_timer_get_time();
    while (!gpio_get_level(ECHO_GPIO)) {
        if ((esp_timer_get_time() - start_time) > TIMEOUT_US) {
            return 0;
        }
    }
    int64_t high_start = esp_timer_get_time();
    while (gpio_get_level(ECHO_GPIO)) {
        if ((esp_timer_get_time() - high_start) > TIMEOUT_US) {
            return 0;
        }
    }
    int64_t high_end = esp_timer_get_time();
    return (uint32_t)(high_end - high_start);
}

bool ultrasonic_read_cm(float *distance, float temperature_celsius)
{
    if (distance == NULL) return false;

    // 发送 Trig 脉冲（>10us）
    gpio_set_level(TRIG_GPIO, 1);
    esp_rom_delay_us(20);   // 至少 10us，这里给 20us
    gpio_set_level(TRIG_GPIO, 0);

    uint32_t pulse_width_us = pulse_in_us();
    if (pulse_width_us == 0) {
        ESP_LOGW(TAG, "超声波测距超时");
        return false;
    }

    // 声速补偿：v = 331.3 + 0.606 * T (m/s)
    float speed_of_sound = 331.3f + 0.606f * temperature_celsius;
    // 距离 = (时间 * 声速) / 2，时间单位秒，声速单位米/秒 -> 得到米，再转厘米
    float distance_m = (pulse_width_us / 1000000.0f) * speed_of_sound / 2.0f;
    *distance = distance_m * 100.0f;

    // 盲区检查：模块盲区 23cm，小于 23cm 的值不可靠
    if (*distance < 23.0f) {
        ESP_LOGW(TAG, "距离小于盲区: %.1f cm", *distance);
        // 仍然返回 true，但用户可自行判断
    }

    ESP_LOGD(TAG, "脉宽 %lu us, 温度 %.1f°C, 声速 %.1f m/s, 距离 %.1f cm",
             pulse_width_us, temperature_celsius, speed_of_sound, *distance);
    return true;
}
// 快速排序辅助函数（用于求中位数）
static int cmp_float(const void *a, const void *b) {
    float fa = *(const float*)a;
    float fb = *(const float*)b;
    return (fa > fb) - (fa < fb);
}

bool ultrasonic_read_stable_cm(float *distance, float temperature_celsius, uint8_t samples, float max_deviation_cm)
{
    if (distance == NULL || samples < 3 || samples > 20) return false;

    float buf[20];
    uint8_t count = 0;

    // 连续采样
    for (uint8_t i = 0; i < samples; i++) {
        float d;
        if (ultrasonic_read_cm(&d, temperature_celsius)) {
            // 只保留盲区以上的数据（>=23cm），避免近场干扰
            if (d >= 23.0f && d <= 500.0f) {
                buf[count++] = d;
            }
        }
        // 两次测量之间至少间隔 50ms，避免回声干扰
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (count < 3) {
        ESP_LOGW("Ultrasonic", "有效样本不足 %d", count);
        return false;
    }

    // 先排序，找到中位数（用于离群值剔除）
    qsort(buf, count, sizeof(float), cmp_float);
    float median = buf[count / 2];

    // 剔除与中位数偏差超过 max_deviation_cm 的值
    uint8_t valid_count = 0;
    float valid_buf[20];
    for (uint8_t i = 0; i < count; i++) {
        if (fabs(buf[i] - median) <= max_deviation_cm) {
            valid_buf[valid_count++] = buf[i];
        }
    }

    if (valid_count < 3) {
        // 如果剔除后不足3个，回退到简单中位数
        *distance = median;
        ESP_LOGW("Ultrasonic", "剔除后样本不足，使用中位数 %.1f cm", median);
        return true;
    }

    // 对有效值再次排序，取中位数
    qsort(valid_buf, valid_count, sizeof(float), cmp_float);
    *distance = valid_buf[valid_count / 2];
    ESP_LOGD("Ultrasonic", "稳定距离: %.1f cm (采样数=%d, 有效=%d)", *distance, samples, valid_count);
    return true;
}
