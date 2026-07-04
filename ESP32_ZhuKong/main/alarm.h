// #ifndef ALARM_H
// #define ALARM_H

// #include <stdbool.h>
// #include "esp_err.h"

// /**
//  * @brief 初始化报警 GPIO（默认低电平，无报警）
//  * @param gpio_num GPIO 引脚号，例如 48
//  * @return ESP_OK 成功
//  */
// esp_err_t alarm_init(int gpio_num);

// /**
//  * @brief 设置报警状态
//  * @param active true：输出高电平（报警），false：输出低电平（正常）
//  */
// void alarm_set(bool active);

// /**
//  * @brief 获取当前报警状态
//  * @return true 报警中，false 正常
//  */
// bool alarm_get(void);

// #endif // ALARM_H
// #ifndef ALARM_H
// #define ALARM_H

// #include <stdbool.h>
// #include "esp_err.h"

// // 报警类型
// typedef enum {
//     ALARM_TYPE_ROOF,     // 顶板位移报警 (GPIO48)
//     ALARM_TYPE_METHANE   // 甲烷浓度报警 (GPIO45)
// } alarm_type_t;

// /**
//  * @brief 初始化所有报警 GPIO
//  * @return ESP_OK 成功
//  */
// esp_err_t alarm_init(void);

// /**
//  * @brief 设置指定类型的报警状态
//  * @param type 报警类型
//  * @param active true: 输出高电平（报警），false: 输出低电平（正常）
//  */
// void alarm_set(alarm_type_t type, bool active);

// /**
//  * @brief 获取指定类型的报警状态
//  * @param type 报警类型
//  * @return true 报警中，false 正常
//  */
// bool alarm_get(alarm_type_t type);

// #endif // ALARM_H
#ifndef ALARM_H
#define ALARM_H

#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    ALARM_TYPE_ROOF,      // 顶板距离 (GPIO48,9)
    ALARM_TYPE_METHANE,   // 甲烷浓度 (GPIO45,10)
    ALARM_TYPE_PM25       // PM2.5 浓度 (GPIO35,11)
} alarm_type_t;

esp_err_t alarm_init(void);
void alarm_set(alarm_type_t type, bool active);
bool alarm_get(alarm_type_t type);

#endif
