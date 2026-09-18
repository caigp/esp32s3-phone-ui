// periph_manager.h
#pragma once

#include "driver/spi_master.h"
#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "driver/i2c_master.h"
#include "esp_vfs_fat.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "sdmmc_cmd.h"
#include "esp_player.h"
#include "driver/i2s_std.h"
#include <math.h>
#include <esp_heap_caps.h>
#include "player_wrapper.h"
#include <dirent.h>

#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

#include "board.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_ft6x36.h"
#include "esp_spiffs.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LVGL_DRAW_BUF_LINES             (80)

#define SAMPLE_RATE    44100
#define SLOT_BITS      I2S_DATA_BIT_WIDTH_16BIT
#define CHANNEL        I2S_SLOT_MODE_MONO

// === I2C ===
extern i2c_master_bus_handle_t g_i2c_bus;

// === SPI ===
extern esp_lcd_panel_handle_t panel_handle;
extern esp_lcd_panel_io_handle_t io_handle;
extern esp_lcd_touch_handle_t tp;

// === I2S ===
extern i2s_chan_handle_t tx_chan;

// 初始化所有外设（按需调用）
void periph_manager_init(void);

#ifdef __cplusplus
}
#endif