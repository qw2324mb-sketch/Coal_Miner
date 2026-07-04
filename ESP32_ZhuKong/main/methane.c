#include "methane.h"
#include "driver/adc.h"
#include "esp_log.h"

static const char *TAG = "METHANE";

// 配置 ADC
#define METHANE_ADC_CHANNEL ADC1_CHANNEL_4  // GPIO5
#define METHANE_ADC_WIDTH   ADC_WIDTH_BIT_12
#define METHANE_ADC_ATTEN   ADC_ATTEN_DB_11  // 0~3.3V

// 标定参数：假设 0V = 0%LEL, 3.3V = 100%LEL（需实际标定）
#define VOLT_TO_LEL_SLOPE   (100.0f / 3.3f)

esp_err_t methane_init(void) {
    adc1_config_width(METHANE_ADC_WIDTH);
    adc1_config_channel_atten(METHANE_ADC_CHANNEL, METHANE_ADC_ATTEN);
    ESP_LOGI(TAG, "初始化完成，ADC通道: %d", METHANE_ADC_CHANNEL);
    return ESP_OK;
}

bool methane_read(float *concentration_lel) {
    if (concentration_lel == NULL) return false;
    int raw = adc1_get_raw(METHANE_ADC_CHANNEL);
    float voltage = raw * (3.3f / 4095.0f);
    *concentration_lel = voltage * VOLT_TO_LEL_SLOPE;
    if (*concentration_lel < 0) *concentration_lel = 0;
    if (*concentration_lel > 100) *concentration_lel = 100;
    ESP_LOGD(TAG, "Raw=%d, Voltage=%.2fV, Concentration=%.1f%%LEL", raw, voltage, *concentration_lel);
    return true;
}
