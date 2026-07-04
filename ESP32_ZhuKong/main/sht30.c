// sht30.c
#include "sht30.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "SHT30";

// CRC-8 校验：多项式 0x31 (x8 + x5 + x4 + 1), 初始值 0xFF
static uint8_t sht30_crc8(const uint8_t *data, size_t len)
{
    uint8_t crc = 0xFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    return crc;
}

esp_err_t sht30_init(void)
{
    // 配置 I2C 参数
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SHT30_I2C_MASTER_SDA_GPIO,
        .scl_io_num = SHT30_I2C_MASTER_SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = SHT30_I2C_MASTER_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(SHT30_I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) return err;
    err = i2c_driver_install(SHT30_I2C_MASTER_NUM, conf.mode, 0, 0, 0);
    if (err != ESP_OK) return err;

    // 可选的软复位（发送软复位命令 0x30A2）
    uint8_t soft_reset_cmd[2] = {0x30, 0xA2};
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (SHT30_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, soft_reset_cmd, 2, true);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(SHT30_I2C_MASTER_NUM, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "软复位失败，继续尝试初始化");
    }
    vTaskDelay(pdMS_TO_TICKS(10)); // 等待复位完成

    ESP_LOGI(TAG, "SHT30 初始化完成");
    return ESP_OK;
}

esp_err_t sht30_read_measurement(float *temperature, float *humidity)
{
    if (!temperature || !humidity) return ESP_ERR_INVALID_ARG;

    uint8_t cmd[2] = { (SHT30_CMD_MEAS_HREP >> 8) & 0xFF, SHT30_CMD_MEAS_HREP & 0xFF };
    uint8_t data[6] = {0}; // 存放原始数据：温度 MSB, LSB, CRC, 湿度 MSB, LSB, CRC

    // 步骤1: 发送测量命令
    i2c_cmd_handle_t cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (SHT30_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd_handle, cmd, 2, true);
    i2c_master_stop(cmd_handle);
    esp_err_t err = i2c_master_cmd_begin(SHT30_I2C_MASTER_NUM, cmd_handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "发送测量命令失败");
        return err;
    }

    // 步骤2: 等待测量完成（高重复性典型 15ms，最坏 15.5ms）
    vTaskDelay(pdMS_TO_TICKS(20));

    // 步骤3: 读取数据（6 字节）
    cmd_handle = i2c_cmd_link_create();
    i2c_master_start(cmd_handle);
    i2c_master_write_byte(cmd_handle, (SHT30_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd_handle, data, 6, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd_handle);
    err = i2c_master_cmd_begin(SHT30_I2C_MASTER_NUM, cmd_handle, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "读取数据失败");
        return err;
    }

    // 步骤4: CRC 校验
    uint8_t calc_crc_t = sht30_crc8(data, 2);      // 温度数据的 CRC
    uint8_t calc_crc_h = sht30_crc8(data + 3, 2);  // 湿度数据的 CRC
    if (calc_crc_t != data[2] || calc_crc_h != data[5]) {
        ESP_LOGE(TAG, "CRC 校验失败: T_CRC=0x%02X (calc=0x%02X), H_CRC=0x%02X (calc=0x%02X)",
                 data[2], calc_crc_t, data[5], calc_crc_h);
        return ESP_FAIL;
    }

    // 步骤5: 转换成物理值
    uint16_t raw_t = (data[0] << 8) | data[1];
    uint16_t raw_h = (data[3] << 8) | data[4];
    *temperature = -45.0f + 175.0f * (raw_t / 65535.0f);
    *humidity    = 100.0f * (raw_h / 65535.0f);

    ESP_LOGD(TAG, "Raw T=0x%04X (%d), Raw H=0x%04X (%d)", raw_t, raw_t, raw_h, raw_h);
    return ESP_OK;
}
