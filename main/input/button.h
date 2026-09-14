#pragma once

#include "iot_button.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "button_gpio.h"

extern void button_init(void);
extern void button_deinit(void);