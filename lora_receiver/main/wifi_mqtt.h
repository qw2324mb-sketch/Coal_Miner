#ifndef WIFI_MQTT_H
#define WIFI_MQTT_H

#include "esp_err.h"

esp_err_t wifi_mqtt_init(void);
esp_err_t wifi_mqtt_publish(const char *topic, const char *data);

#endif