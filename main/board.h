#pragma once
#include "driver/gpio.h"

#define LCD_HOST        SPI2_HOST
#define TOUCH_HOST      SPI2_HOST

#define LCD_H_RES               240
#define LCD_V_RES               320
#define LCD_BIT_PER_PIXEL       (16)
#define LCD_PIXEL_CLOCK_HZ      (40 * 1000 * 1000)


#define PIN_NUM_SCLK    40
#define PIN_NUM_MOSI    41
#define PIN_NUM_MISO    38

#define PIN_NUM_LCD_CS  1
#define PIN_NUM_LCD_DC  42
#define PIN_NUM_LCD_RST 2
#define PIN_NUM_LCD_BL  39

#define PIN_NUM_TOUCH_CS   47
#define PIN_NUM_TOUCH_IRQ  GPIO_NUM_NC


#define I2C_MASTER_SCL_IO   21   /*!< GPIO number for I2C master clock */
#define I2C_MASTER_SDA_IO   47   /*!< GPIO number for I2C master data  */
#define I2C_MASTER_NUM      I2C_NUM_0               /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ  100000                  /*!< I2C master clock frequency */

/* 内存卡模块 */
#define SD_SCK              6
#define SD_MISO             7
#define SD_MOSI             5
#define SD_CS               4
#define SD_MOUNT_PATH       "/sdcard"
#define DIR_MUSIC           SD_MOUNT_PATH "/Music"
#define DIR_GAME            SD_MOUNT_PATH "/Game"
#define DIR_RECORD          SD_MOUNT_PATH "/Record"
#define DIR_MOVIE           SD_MOUNT_PATH "/Movie"


#define SPK_BCLK_GPIO  GPIO_NUM_14
#define SPK_WS_GPIO    GPIO_NUM_13
#define SPK_DOUT_GPIO  GPIO_NUM_10   // MAX98357 DIN
#define MIC_DINT_GPIO  GPIO_NUM_12