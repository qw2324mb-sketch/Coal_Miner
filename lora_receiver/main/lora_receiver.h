// #ifndef LORA_RECEIVER_H
// #define LORA_RECEIVER_H

// #include <stdbool.h>
// #include "esp_err.h"

// esp_err_t lora_receiver_init(void);
// void lora_receiver_start_task(void);


// #endif
#ifndef LORA_RECEIVER_H
#define LORA_RECEIVER_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t lora_receiver_init(void);
void lora_receiver_start_task(void);

// 新增：回调函数类型和注册函数
typedef void (*lora_data_callback_t)(const char *json_str);
void lora_receiver_register_callback(lora_data_callback_t cb);


#endif
