// pm25.h
#ifndef PM25_H
#define PM25_H

#include <stdbool.h>   
#include "esp_err.h"
#include "driver/uart.h"

// UART 配置
#define PM25_UART_NUM       UART_NUM_1
#define PM25_UART_TX_PIN    -1           // 本例只接收，不需要 TX
#define PM25_UART_RX_PIN    4           //传感器的TX
#define PM25_UART_BAUD_RATE 9600

/**
 * @brief 初始化 PM2.5 串口传感器
 * @return ESP_OK 成功，其他失败
 */
esp_err_t pm25_init(void);

/**
 * @brief 获取最新的 PM2.5 浓度值 (µg/m³)
 * @param concentration 输出浓度
 * @return true 表示有有效新数据，false 表示无新数据或无效
 */
bool pm25_get_concentration(float *concentration);

// pm25.h 中添加
void pm25_start_task(void);   // 启动后台接收任务，持续更新全局浓度
float pm25_get_latest(void);  // 获取最新浓度（非阻塞）

#endif
