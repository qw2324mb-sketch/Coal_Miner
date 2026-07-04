#ifndef MQ4_H
#define MQ4_H

#include <stdbool.h>
#include "esp_err.h"

/**
 * @brief 初始化 MQ-4 传感器的 ADC 通道
 * @return ESP_OK 成功
 */
esp_err_t mq4_init(void);

/**
 * @brief 校准传感器的 R0 值（洁净空气中）
 * @param samples 采样次数（建议 10~20 次）
 * @note 必须在传感器预热稳定后调用（预热至少 2 小时，演示时可缩短）
 */
void mq4_calibrate_r0(int samples);

/**
 * @brief 读取当前甲烷浓度（%LEL）
 * @param concentration_lel 输出浓度（0~100 %LEL）
 * @return true 成功，false 失败
 */
bool mq4_read(float *concentration_lel);

#endif // MQ4_H