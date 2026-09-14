#pragma once

#include <driver/i2c_master.h>
#include "esp_types.h"
#include "esp_err.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>



#define BMP280_ADDR 0x77

/* Internal calibration data + device handles; declare on the stack or statically. */
typedef struct {
    i2c_master_dev_handle_t bmp280;
    /* BMP280 trimming coefficients */
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
} env_sensor_t;

esp_err_t bmp280_new_sensor(i2c_master_bus_handle_t bus, env_sensor_t *handle);

esp_err_t bmp280_del_sensor(env_sensor_t *handle);

esp_err_t bmp280_read(i2c_master_dev_handle_t dev, env_sensor_t *h, float *pressure);