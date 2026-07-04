// #include "alarm.h"
// #include "driver/gpio.h"
// #include "esp_log.h"

// static const char *TAG = "ALARM";
// static int s_alarm_gpio = -1;
// static bool s_alarm_active = false;

// esp_err_t alarm_init(int gpio_num) {
//     if (gpio_num < 0) return ESP_ERR_INVALID_ARG;
//     s_alarm_gpio = gpio_num;

//     gpio_config_t conf = {
//         .pin_bit_mask = (1ULL << s_alarm_gpio),
//         .mode = GPIO_MODE_OUTPUT,
//         .intr_type = GPIO_INTR_DISABLE,
//         .pull_down_en = GPIO_PULLDOWN_DISABLE,
//         .pull_up_en = GPIO_PULLUP_DISABLE,
//     };
//     esp_err_t err = gpio_config(&conf);
//     if (err != ESP_OK) return err;

//     // 初始状态：无报警（低电平）
//     gpio_set_level(s_alarm_gpio, 0);
//     s_alarm_active = false;
//     ESP_LOGI(TAG, "报警 GPIO%d 初始化完成，初始低电平", s_alarm_gpio);
//     return ESP_OK;
// }

// void alarm_set(bool active) {
//     if (s_alarm_gpio < 0) {
//         ESP_LOGE(TAG, "报警未初始化，无法设置状态");
//         return;
//     }
//     s_alarm_active = active;
//     gpio_set_level(s_alarm_gpio, active ? 1 : 0);
//     ESP_LOGD(TAG, "报警状态: %s", active ? "激活(高电平)" : "解除(低电平)");
// }

// bool alarm_get(void) {
//     return s_alarm_active;
// }
// #include "alarm.h"
// #include "driver/gpio.h"
// #include "esp_log.h"

// static const char *TAG = "ALARM";

// // GPIO 定义
// #define GPIO_ROOF     48
// #define GPIO_METHANE  45

// static bool s_roof_active = false;
// static bool s_methane_active = false;

// esp_err_t alarm_init(void) {
//     // 初始化顶板报警 GPIO
//     gpio_config_t roof_conf = {
//         .pin_bit_mask = (1ULL << GPIO_ROOF),
//         .mode = GPIO_MODE_OUTPUT,
//         .intr_type = GPIO_INTR_DISABLE,
//         .pull_down_en = GPIO_PULLDOWN_DISABLE,
//         .pull_up_en = GPIO_PULLUP_DISABLE,
//     };
//     esp_err_t err = gpio_config(&roof_conf);
//     if (err != ESP_OK) return err;
//     gpio_set_level(GPIO_ROOF, 0);

//     // 初始化甲烷报警 GPIO
//     gpio_config_t methane_conf = {
//         .pin_bit_mask = (1ULL << GPIO_METHANE),
//         .mode = GPIO_MODE_OUTPUT,
//         .intr_type = GPIO_INTR_DISABLE,
//         .pull_down_en = GPIO_PULLDOWN_DISABLE,
//         .pull_up_en = GPIO_PULLUP_DISABLE,
//     };
//     err = gpio_config(&methane_conf);
//     if (err != ESP_OK) return err;
//     gpio_set_level(GPIO_METHANE, 0);

//     ESP_LOGI(TAG, "报警模块初始化完成: 顶板=GPIO%d, 甲烷=GPIO%d", GPIO_ROOF, GPIO_METHANE);
//     return ESP_OK;
// }

// void alarm_set(alarm_type_t type, bool active) {
//     switch (type) {
//         case ALARM_TYPE_ROOF:
//             s_roof_active = active;
//             gpio_set_level(GPIO_ROOF, active ? 1 : 0);
//             ESP_LOGD(TAG, "顶板报警: %s", active ? "激活" : "解除");
//             break;
//         case ALARM_TYPE_METHANE:
//             s_methane_active = active;
//             gpio_set_level(GPIO_METHANE, active ? 1 : 0);
//             ESP_LOGD(TAG, "甲烷报警: %s", active ? "激活" : "解除");
//             break;
//         default:
//             ESP_LOGE(TAG, "未知报警类型");
//             break;
//     }
// }

// bool alarm_get(alarm_type_t type) {
//     switch (type) {
//         case ALARM_TYPE_ROOF:   return s_roof_active;
//         case ALARM_TYPE_METHANE: return s_methane_active;
//         default:                return false;
//     }
// }
#include "alarm.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "ALARM";

// ========== 引脚定义 ==========
// 顶板报警 (超声波)
#define ROOF_GPIO1   48
#define ROOF_GPIO2   9

// 甲烷报警
#define METHANE_GPIO1 45
#define METHANE_GPIO2 10

// PM2.5 报警
#define PM25_GPIO1    35
#define PM25_GPIO2    11
// =============================

static bool s_roof_active = false;
static bool s_methane_active = false;
static bool s_pm25_active = false;

// 通用 GPIO 输出初始化
static esp_err_t init_gpio_out(uint64_t pin_bit_mask) {
    gpio_config_t io_conf = {
        .pin_bit_mask = pin_bit_mask,
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    return gpio_config(&io_conf);
}

esp_err_t alarm_init(void) {
    // 顶板两个引脚
    ESP_ERROR_CHECK(init_gpio_out((1ULL << ROOF_GPIO1) | (1ULL << ROOF_GPIO2)));
    // 甲烷两个引脚
    ESP_ERROR_CHECK(init_gpio_out((1ULL << METHANE_GPIO1) | (1ULL << METHANE_GPIO2)));
    // PM2.5 两个引脚
    ESP_ERROR_CHECK(init_gpio_out((1ULL << PM25_GPIO1) | (1ULL << PM25_GPIO2)));

    // 初始低电平
    gpio_set_level(ROOF_GPIO1, 0);
    gpio_set_level(ROOF_GPIO2, 0);
    gpio_set_level(METHANE_GPIO1, 0);
    gpio_set_level(METHANE_GPIO2, 0);
    gpio_set_level(PM25_GPIO1, 0);
    gpio_set_level(PM25_GPIO2, 0);

    ESP_LOGI(TAG, "报警模块初始化完成");
    ESP_LOGI(TAG, "顶板: GPIO%d, GPIO%d", ROOF_GPIO1, ROOF_GPIO2);
    ESP_LOGI(TAG, "甲烷: GPIO%d, GPIO%d", METHANE_GPIO1, METHANE_GPIO2);
    ESP_LOGI(TAG, "PM2.5: GPIO%d, GPIO%d", PM25_GPIO1, PM25_GPIO2);
    return ESP_OK;
}

void alarm_set(alarm_type_t type, bool active) {
    switch (type) {
        case ALARM_TYPE_ROOF:
            s_roof_active = active;
            gpio_set_level(ROOF_GPIO1, active ? 1 : 0);
            gpio_set_level(ROOF_GPIO2, active ? 1 : 0);
            ESP_LOGD(TAG, "顶板报警: %s", active ? "激活" : "解除");
            break;
        case ALARM_TYPE_METHANE:
            s_methane_active = active;
            gpio_set_level(METHANE_GPIO1, active ? 1 : 0);
            gpio_set_level(METHANE_GPIO2, active ? 1 : 0);
            ESP_LOGD(TAG, "甲烷报警: %s", active ? "激活" : "解除");
            break;
        case ALARM_TYPE_PM25:
            s_pm25_active = active;
            gpio_set_level(PM25_GPIO1, active ? 1 : 0);
            gpio_set_level(PM25_GPIO2, active ? 1 : 0);
            ESP_LOGD(TAG, "PM2.5报警: %s", active ? "激活" : "解除");
            break;
        default:
            ESP_LOGE(TAG, "未知报警类型");
            break;
    }
}

bool alarm_get(alarm_type_t type) {
    switch (type) {
        case ALARM_TYPE_ROOF:   return s_roof_active;
        case ALARM_TYPE_METHANE: return s_methane_active;
        case ALARM_TYPE_PM25:    return s_pm25_active;
        default:                return false;
    }
}