// sht30.h
#ifndef SHT30_H
#define SHT30_H

#include <stdint.h>
#include "esp_err.h"

// I2C 配置参数
#define SHT30_I2C_MASTER_NUM      0           // I2C 端口号
#define SHT30_I2C_MASTER_SDA_GPIO 19          // 连接 SHT30 SDA
#define SHT30_I2C_MASTER_SCL_GPIO 20          // 连接 SHT30 SCL
#define SHT30_I2C_MASTER_FREQ_HZ  100000      // I2C 时钟 100kHz

// SHT30 设备地址（根据 ADDR 引脚连接选择）
#define SHT30_ADDR                0x44        // ADDR 接地
// #define SHT30_ADDR              0x45        // ADDR 接高电平

// 测量命令（高重复性，无时钟拉伸）
#define SHT30_CMD_MEAS_HREP       0x2400

/**
 * @brief 初始化 SHT30 传感器（初始化 I2C 总线）
 * @return ESP_OK 成功；其他值表示失败
 */
esp_err_t sht30_init(void);

/**
 * @brief 读取温湿度数据（阻塞，等待测量完成）
 * @param temperature 温度值（摄氏度），指针传入
 * @param humidity    湿度值（%RH），指针传入
 * @return ESP_OK 成功；ESP_FAIL 通信或 CRC 错误
 */
esp_err_t sht30_read_measurement(float *temperature, float *humidity);

#endif // SHT30_H
