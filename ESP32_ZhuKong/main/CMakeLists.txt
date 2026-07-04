idf_component_register(SRCS "mq4.c" "lora.c" "main.c" "sht30.c" "pm25.c" "ultrasonic.c" "alarm.c"  "mq4.c"
                       INCLUDE_DIRS "."
                       PRIV_REQUIRES driver esp_timer)