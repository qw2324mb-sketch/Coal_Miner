#ifndef METHANE_H
#define METHANE_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t methane_init(void);
bool methane_read(float *concentration_lel);  // 返回 %LEL (爆炸下限百分比)

#endif
