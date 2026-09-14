#include "bmp280.h"

const static char *TAG = "BMP280";

/* ──────────────────────────── BMP280 ──────────────────────────── */

#define BMP280_REG_CHIP_ID   0xD0
#define BMP280_REG_RESET     0xE0
#define BMP280_REG_CTRL_MEAS 0xF4
#define BMP280_REG_CONFIG    0xF5
#define BMP280_REG_PRESS_MSB 0xF7
#define BMP280_REG_CALIB     0x88

static esp_err_t bmp280_read_regs(i2c_master_dev_handle_t dev, uint8_t reg,
                                   uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(dev, &reg, 1, buf, len, 100);
}

static esp_err_t bmp280_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[] = {reg, val};
    return i2c_master_transmit(dev, buf, 2, 100);
}

static esp_err_t bmp280_init(i2c_master_dev_handle_t dev, env_sensor_t *h)
{
    uint8_t id;
    esp_err_t ret = bmp280_read_regs(dev, BMP280_REG_CHIP_ID, &id, 1);
    if (ret != ESP_OK) return ret;
    /* 0x58 = BMP280, 0x60 = BME280 (pressure-compatible) */
    if (id != 0x58 && id != 0x60) {
        ESP_LOGE(TAG, "BMP280 unexpected chip ID 0x%02X", id);
        return ESP_ERR_INVALID_RESPONSE;
    }

    ret = bmp280_write_reg(dev, BMP280_REG_RESET, 0xB6);
    if (ret != ESP_OK) return ret;
    vTaskDelay(pdMS_TO_TICKS(10));

    uint8_t cal[24];
    ret = bmp280_read_regs(dev, BMP280_REG_CALIB, cal, sizeof(cal));
    if (ret != ESP_OK) return ret;

#define LE16U(i) ((uint16_t)((cal[(i)] | ((uint16_t)cal[(i)+1] << 8))))
#define LE16S(i) ((int16_t) LE16U(i))
    h->dig_T1 = LE16U(0);  h->dig_T2 = LE16S(2);  h->dig_T3 = LE16S(4);
    h->dig_P1 = LE16U(6);  h->dig_P2 = LE16S(8);  h->dig_P3 = LE16S(10);
    h->dig_P4 = LE16S(12); h->dig_P5 = LE16S(14); h->dig_P6 = LE16S(16);
    h->dig_P7 = LE16S(18); h->dig_P8 = LE16S(20); h->dig_P9 = LE16S(22);
#undef LE16U
#undef LE16S

    /* osrs_t=x2, osrs_p=x16, normal mode */
    ret = bmp280_write_reg(dev, BMP280_REG_CTRL_MEAS, 0x57);
    if (ret != ESP_OK) return ret;
    /* IIR filter coeff=16, standby=0.5 ms */
    ret = bmp280_write_reg(dev, BMP280_REG_CONFIG, 0x10);
    if (ret != ESP_OK) return ret;

    vTaskDelay(pdMS_TO_TICKS(50));   /* first measurement ready */
    return ESP_OK;
}

esp_err_t bmp280_read(i2c_master_dev_handle_t dev, env_sensor_t *h, float *pressure)
{
    uint8_t raw[6];
    esp_err_t ret = bmp280_read_regs(dev, BMP280_REG_PRESS_MSB, raw, sizeof(raw));
    if (ret != ESP_OK) return ret;

    int32_t adc_P = ((int32_t)raw[0] << 12) | ((int32_t)raw[1] << 4) | (raw[2] >> 4);
    int32_t adc_T = ((int32_t)raw[3] << 12) | ((int32_t)raw[4] << 4) | (raw[5] >> 4);

    /* Temperature compensation (Bosch datasheet section 4.2.3) */
    int32_t v1 = ((((adc_T >> 3) - ((int32_t)h->dig_T1 << 1))) * h->dig_T2) >> 11;
    int32_t v2 = (((((adc_T >> 4) - (int32_t)h->dig_T1) *
                    ((adc_T >> 4) - (int32_t)h->dig_T1)) >> 12) * h->dig_T3) >> 14;
    int32_t t_fine = v1 + v2;

    /* Pressure compensation — 64-bit integer path */
    int64_t p1 = (int64_t)t_fine - 128000;
    int64_t p2 = p1 * p1 * (int64_t)h->dig_P6;
    p2 = p2 + ((p1 * (int64_t)h->dig_P5) << 17);
    p2 = p2 + (((int64_t)h->dig_P4) << 35);
    p1 = ((p1 * p1 * (int64_t)h->dig_P3) >> 8) + ((p1 * (int64_t)h->dig_P2) << 12);
    p1 = ((((int64_t)1) << 47) + p1) * ((int64_t)h->dig_P1) >> 33;
    if (p1 == 0) return ESP_ERR_INVALID_STATE;
    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - p2) * 3125) / p1;
    p1 = (((int64_t)h->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    p2 = (((int64_t)h->dig_P8) * p) >> 19;
    p = ((p + p1 + p2) >> 8) + (((int64_t)h->dig_P7) << 4);

    /* Result is Pa*256 (Q24.8); convert to hPa */
    *pressure = (float)(uint32_t)p / 256.0f / 100.0f;
    return ESP_OK;
}

esp_err_t bmp280_new_sensor(i2c_master_bus_handle_t bus, env_sensor_t *handle)
{
    esp_err_t ret;

    i2c_device_config_t bmp_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = BMP280_ADDR,
        .scl_speed_hz    = 400000,
    };
    ret = i2c_master_bus_add_device(bus, &bmp_cfg, &handle->bmp280);
    if (ret != ESP_OK) {
        i2c_master_bus_rm_device(handle->bmp280);
        return ret;
    }

    ret = bmp280_init(handle->bmp280, handle);

    return ret;
}

esp_err_t bmp280_del_sensor(env_sensor_t *handle)
{
    return i2c_master_bus_rm_device(handle->bmp280);
}