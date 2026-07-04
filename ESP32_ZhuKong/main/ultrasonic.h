#ifndef ULTRASONIC_H
#define ULTRASONIC_H

#include <stdbool.h>      // <--- 必须添加这一行
#include "esp_err.h"

// 引脚配置（可根据实际接线修改）
#define TRIG_GPIO   21
#define ECHO_GPIO   47

#define TIMEOUT_US  30000

esp_err_t ultrasonic_init(void);

bool ultrasonic_read_cm(float *distance, float temperature_celsius);
/**
 * @brief 多次测量取中值，获得稳定距离
 * @param distance 输出稳定距离（厘米）
 * @param temperature_celsius 当前温度，用于声速补偿
 * @param samples 测量次数（建议 5~9 次，奇数）
 * @param max_deviation_cm 最大允许偏差（厘米），超过此范围的测量值将被剔除
 * @return true 成功，false 失败（有效样本不足）
 */
bool ultrasonic_read_stable_cm(float *distance, float temperature_celsius, uint8_t samples, float max_deviation_cm);
#endif
