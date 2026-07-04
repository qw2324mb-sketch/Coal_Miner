#include "mq4.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "MQ4";

// ADC 配置
#define MQ4_ADC_CHANNEL   ADC1_CHANNEL_4   // GPIO5
#define MQ4_ADC_WIDTH     ADC_WIDTH_BIT_12
#define MQ4_ADC_ATTEN     ADC_ATTEN_DB_11   // 0 ~ 3.3V

// 分压电路参数
#define RL_VAL            20000.0f          // 负载电阻 20kΩ
#define VC_VAL            5.0f              // 回路电压 5V

// 灵敏度曲线拟合系数（甲烷，根据手册图3拟合）
// log10(ppm) = A * log10(Rs/R0) + B
#define COEF_A            -1.4f
#define COEF_B            3.2f

// 甲烷爆炸下限 50000 ppm
#define LEL_PPM           50000.0f

static float s_R0 = 0.0f;
static bool s_R0_calibrated = false;

// 根据采样电压计算 Rs
static float get_rs(float voltage) {
    if (voltage <= 0.0f) return 1e6f;
    return (VC_VAL / voltage - 1.0f) * RL_VAL;
}

// 根据 Rs/R0 比值计算浓度（ppm）
static float ratio_to_ppm(float ratio) {
    if (ratio <= 0.0f) return 0.0f;
    return powf(10.0f, COEF_A * log10f(ratio) + COEF_B);
}

esp_err_t mq4_init(void) {
    adc1_config_width(MQ4_ADC_WIDTH);
    adc1_config_channel_atten(MQ4_ADC_CHANNEL, MQ4_ADC_ATTEN);
    ESP_LOGI(TAG, "MQ-4 初始化完成 (ADC1_CH%d)", MQ4_ADC_CHANNEL);
    return ESP_OK;
}

void mq4_calibrate_r0(int samples) {
    float sum_rs = 0.0f;
    int valid = 0;
    for (int i = 0; i < samples; i++) {
        int raw = adc1_get_raw(MQ4_ADC_CHANNEL);
        float adc_volt = raw * (3.3f / 4095.0f);
        float voltage = adc_volt * (5.0f / 3.3f);
        // 洁净空气中电压通常较低，过滤异常值
        if (voltage > 0.1f && voltage < 2.0f) {
            sum_rs += get_rs(voltage);
            valid++;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (valid > 0) {
        s_R0 = sum_rs / valid;
        s_R0_calibrated = true;
        ESP_LOGI(TAG, "R0 校准完成: %.1f Ω (基于 %d 个样本)", s_R0, valid);
    } else {
        ESP_LOGW(TAG, "校准失败，使用默认 R0 = 10000Ω");
        s_R0 = 10000.0f;
        s_R0_calibrated = true;
    }
}

bool mq4_read(float *concentration_lel) {
    if (!concentration_lel) return false;

    if (!s_R0_calibrated) {
        ESP_LOGW(TAG, "未校准 R0，使用默认值 10000Ω");
        s_R0 = 10000.0f;
        s_R0_calibrated = true;
    }

    int raw = adc1_get_raw(MQ4_ADC_CHANNEL);
    float adc_volt = raw * (3.3f / 4095.0f);
    float voltage = adc_volt * (5.0f / 3.3f);
    float rs = get_rs(voltage);
    float ratio = rs / s_R0;
    float ppm = ratio_to_ppm(ratio);
    *concentration_lel = (ppm / LEL_PPM) * 100.0f;

    // 限幅
    if (*concentration_lel < 0.0f) *concentration_lel = 0.0f;
    if (*concentration_lel > 100.0f) *concentration_lel = 100.0f;

    ESP_LOGD(TAG, "电压=%.2fV, Rs=%.1fΩ, ratio=%.3f, ppm=%.0f, LEL=%.1f%%",
             voltage, rs, ratio, ppm, *concentration_lel);
    return true;
}