/**
 * sx126x_hal_esp32.c - ESP32-S3 硬件抽象层实现（硬编码引脚 + 互斥锁）
 * 引脚定义请根据实际硬件连接修改
 */

#include "sx126x_hal.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "SX126X_HAL";

// ==================== 引脚定义（必须与硬件连接一致）====================
// SPI 接口
#define LORA_NSS_GPIO   5
#define LORA_SCK_GPIO   18
#define LORA_MOSI_GPIO  11
#define LORA_MISO_GPIO  13

// 控制引脚
#define LORA_BUSY_GPIO  14
#define LORA_RST_GPIO   6
#define LORA_DIO1_GPIO  15

static spi_device_handle_t spi_handle = NULL;
static SemaphoreHandle_t spi_mutex = NULL;

// 初始化 SPI 总线
void sx126x_hal_init_spi(void)
{
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = LORA_MOSI_GPIO,
        .miso_io_num = LORA_MISO_GPIO,
        .sclk_io_num = LORA_SCK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256,
    };
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 8 * 1000 * 1000,  // 8 MHz
        .mode = 0,
        .spics_io_num = LORA_NSS_GPIO,
        .queue_size = 7,                    // 增大队列深度
        // .flags = 0,
    };
    // 使用 SPI3_HOST 避免与 PSRAM 冲突
    esp_err_t ret = spi_bus_initialize(SPI3_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return;
    }
    ret = spi_bus_add_device(SPI3_HOST, &dev_cfg, &spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        return;
    }
    // 创建互斥锁
    spi_mutex = xSemaphoreCreateMutex();
    configASSERT(spi_mutex != NULL);
    ESP_LOGI(TAG, "SPI initialized (SPI3_HOST) with mutex");
}

// SPI 写操作（带互斥锁）
sx126x_hal_status_t sx126x_hal_write(const void *context, const uint8_t *command, uint16_t command_length,
                                     const uint8_t *data, uint16_t data_length)
{
    // 等待 BUSY 拉低（超时保护）
    int timeout_ms = 100;
    while (gpio_get_level(LORA_BUSY_GPIO) == 1 && timeout_ms--) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (gpio_get_level(LORA_BUSY_GPIO) == 1) {
        ESP_LOGW(TAG, "BUSY stuck high in write");
        return SX126X_HAL_STATUS_ERROR;
    }

    if (spi_mutex == NULL) return SX126X_HAL_STATUS_ERROR;
    if (xSemaphoreTake(spi_mutex, portMAX_DELAY) != pdTRUE) {
        return SX126X_HAL_STATUS_ERROR;
    }

    // 发送命令
    if (command_length > 0) {
        spi_transaction_t t = {0};
        t.length = command_length * 8;
        t.tx_buffer = command;
        t.rx_buffer = NULL;
        esp_err_t ret = spi_device_transmit(spi_handle, &t);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI write command failed: %s", esp_err_to_name(ret));
            xSemaphoreGive(spi_mutex);
            return SX126X_HAL_STATUS_ERROR;
        }
    }
    // 发送数据
    if (data_length > 0) {
        spi_transaction_t t = {0};
        t.length = data_length * 8;
        t.tx_buffer = data;
        t.rx_buffer = NULL;
        esp_err_t ret = spi_device_transmit(spi_handle, &t);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI write data failed: %s", esp_err_to_name(ret));
            xSemaphoreGive(spi_mutex);
            return SX126X_HAL_STATUS_ERROR;
        }
    }

    xSemaphoreGive(spi_mutex);
    return SX126X_HAL_STATUS_OK;
}

// SPI 读操作（带互斥锁）
sx126x_hal_status_t sx126x_hal_read(const void *context, const uint8_t *command, uint16_t command_length,
                                    uint8_t *data, uint16_t data_length)
{
    // 等待 BUSY 拉低
    int timeout_ms = 100;
    while (gpio_get_level(LORA_BUSY_GPIO) == 1 && timeout_ms--) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (gpio_get_level(LORA_BUSY_GPIO) == 1) {
        ESP_LOGW(TAG, "BUSY stuck high in read");
        return SX126X_HAL_STATUS_ERROR;
    }

    if (spi_mutex == NULL) return SX126X_HAL_STATUS_ERROR;
    if (xSemaphoreTake(spi_mutex, portMAX_DELAY) != pdTRUE) {
        return SX126X_HAL_STATUS_ERROR;
    }

    // 发送命令
    if (command_length > 0) {
        spi_transaction_t t = {0};
        t.length = command_length * 8;
        t.tx_buffer = command;
        t.rx_buffer = NULL;
        esp_err_t ret = spi_device_transmit(spi_handle, &t);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI read command failed: %s", esp_err_to_name(ret));
            xSemaphoreGive(spi_mutex);
            return SX126X_HAL_STATUS_ERROR;
        }
    }
    // 读取数据
    if (data_length > 0) {
        spi_transaction_t t = {0};
        t.length = data_length * 8;
        t.tx_buffer = NULL;      // 发送 dummy
        t.rx_buffer = data;
        esp_err_t ret = spi_device_transmit(spi_handle, &t);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "SPI read data failed: %s", esp_err_to_name(ret));
            xSemaphoreGive(spi_mutex);
            return SX126X_HAL_STATUS_ERROR;
        }
    }

    xSemaphoreGive(spi_mutex);
    return SX126X_HAL_STATUS_OK;
}

// 硬件复位
sx126x_hal_status_t sx126x_hal_reset(const void *context)
{
    gpio_set_level(LORA_RST_GPIO, 0);
    esp_rom_delay_us(100);
    gpio_set_level(LORA_RST_GPIO, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    return SX126X_HAL_STATUS_OK;
}

// 唤醒模块
sx126x_hal_status_t sx126x_hal_wakeup(const void *context)
{
    // 注意：此函数可能被其他任务调用，但通常只在初始化时调用，不加锁也可，但为安全仍可加锁
    // 拉低 CS，发送 0xC0 0x00
    gpio_set_level(LORA_NSS_GPIO, 0);
    uint8_t cmd[] = {0xC0, 0x00};
    sx126x_hal_write(context, cmd, 2, NULL, 0);
    gpio_set_level(LORA_NSS_GPIO, 1);
    return SX126X_HAL_STATUS_OK;
}

// 获取 BUSY 引脚状态
int sx126x_get_busy(void)
{
    return gpio_get_level(LORA_BUSY_GPIO);
}
