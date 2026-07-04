#ifndef LORA_H
#define LORA_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t lora_init(void);
bool lora_send_data(const uint8_t *data, size_t len);
bool lora_send_string(const char *str);

#endif
