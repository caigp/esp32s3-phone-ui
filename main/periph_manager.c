// periph_manager.c
#include "periph_manager.h"


static const char *TAG = "periph_manager";

i2c_master_bus_handle_t g_i2c_bus = NULL;
esp_lcd_panel_handle_t panel_handle = NULL;
esp_lcd_panel_io_handle_t io_handle = NULL;

i2s_chan_handle_t tx_chan = NULL;

esp_lcd_touch_handle_t tp;

/* XPT2046 */
static void touch_init()
{

    // esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    // esp_lcd_panel_io_spi_config_t tp_io_config =

    // ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(PIN_NUM_TOUCH_CS);

    // // Attach the TOUCH to the SPI bus
    // ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)TOUCH_HOST, &tp_io_config, &tp_io_handle));

    // esp_lcd_touch_config_t tp_cfg = {
    //     .x_max = LCD_H_RES,
    //     .y_max = LCD_V_RES,
    //     .rst_gpio_num = -1,
    //     .int_gpio_num = PIN_NUM_TOUCH_IRQ,
    //     .flags = {
    //         .swap_xy = 0,
    //         .mirror_x = 0,
    //         .mirror_y = 1,
    //     },
    // };

    // ESP_LOGI(TAG, "Initialize touch controller XPT2046");
    // ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(tp_io_handle, &tp_cfg, &tp));
    
    esp_lcd_panel_io_handle_t tp_io_handle;
    
    esp_lcd_panel_io_i2c_config_t io_config = ESP_LCD_TOUCH_IO_I2C_FT6x36_CONFIG();
    io_config.scl_speed_hz = 100000;
    esp_lcd_new_panel_io_i2c(g_i2c_bus, &io_config, &tp_io_handle);

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = 11,
        .int_gpio_num = -1,
        // .levels = {
        //     .reset = 1,
        //     .interrupt = 0,
        // },
        // .flags = {
        //     .swap_xy = 0,
        //     .mirror_x = 0,
        //     .mirror_y = 0,
        // },
    };

    esp_lcd_touch_new_i2c_ft6x36(tp_io_handle, &tp_cfg, &tp);
    
    /* 触摸阈值 */
    esp_lcd_panel_io_tx_param(tp_io_handle, 0x80, (uint8_t[]) {0x40}, 1);
    /* peak阈值 */
    esp_lcd_panel_io_tx_param(tp_io_handle, 0x81, (uint8_t[]) {0x08}, 1);
}


void lcd_fill_red(esp_lcd_panel_handle_t panel)
{
    size_t sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t);
    uint16_t *buf = heap_caps_malloc(sz, MALLOC_CAP_DMA);
    if (!buf) {
        ESP_LOGE("LCD", "malloc fail");
        return;
    }

    for (int i = 0; i < LCD_H_RES * LCD_V_RES; i++) {
        buf[i] = 0xff00;
    }

    esp_lcd_panel_draw_bitmap(panel, 0, 0, LCD_H_RES, LCD_V_RES, buf);

    free(buf);
}

static void lcd_init()
{
    ESP_LOGI(TAG, "Install panel IO");
    
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_LCD_DC,
        .cs_gpio_num = PIN_NUM_LCD_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .flags = {
            
        }
    };
    // Attach the LCD to the SPI bus
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_LOGI(TAG, "Install ST7789 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));

    // user can flush pre-defined pattern to the screen before we turn on the screen or backlight
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // 0x20: Display Inversion OFF (关闭硬件反色)
    // 0x21: Display Inversion ON  (开启硬件反色)
    esp_lcd_panel_io_tx_param(io_handle, 0x21, NULL, 0);
    // 0x55 表示 16 bits/pixel (RGB565)
    // esp_lcd_panel_io_tx_param(io_handle, 0x3A, (uint8_t[]){ 0x55 }, 1);

    // 0x55 表示 MCU 接口和 RGB 接口均使用 16-bit/pixel (RGB565)
    // uint8_t pixel_format = 0x55;
    // esp_lcd_panel_io_tx_param(io_handle, 0x3A, &pixel_format, 1);

    // // 发送 Gamma Curve Set 指令 (0x26)，选择默认 Gamma 曲线 1
    // uint8_t gamma_curve = 0x01;// 可尝试 0x01, 0x02, 0x04, 0x08
    // esp_lcd_panel_io_tx_param(io_handle, 0x26, &gamma_curve, 1);

    // // 降低 VCOM 电压，通常能让“发白”的画面变沉稳、黑色更纯粹
    // // 参数范围一般在 0x00 ~ 0x7F 之间，可以尝试在 0x1A 到 0x3E 之间微调
    // uint8_t vcom_setting[] = { 0x2B, 0x2B }; // 默认值通常在 0x3E 左右，适当调小该值
    // esp_lcd_panel_io_tx_param(io_handle, 0xC5, vcom_setting, 2);

    // // 1. 开启正常显示模式 (Normal Display Mode On)
    // esp_lcd_panel_io_tx_param(io_handle, 0x13, NULL, 0);

    // // 2. 配置 Frame Rate 控制 (0xB1)，降低或升高刷新率有时能改善泛白
    // // 默认为 0x00, 0x1B (约 70Hz)
    // uint8_t frame_rate[] = { 0x00, 0x18 }; 
    // esp_lcd_panel_io_tx_param(io_handle, 0xB1, frame_rate, 2);

    // esp_lcd_panel_invert_color(panel_handle, true);
    // lcd_fill_red(panel_handle);
}

static void sd_test_rw()
{
    // 6. 写测试
    FILE *f = fopen(SD_MOUNT_PATH "/hello.txt", "w");
    if (f) {
        fprintf(f, "esp32-s3 spi3 sd test\n");
        fclose(f);
    }

    // 7. 读测试
    char line[64];
    f = fopen(SD_MOUNT_PATH "/hello.txt", "r");
    if (f) {
        while (fgets(line, sizeof(line), f)) printf("read: %s", line);
        fclose(f);
    }
}

static void sd_test_audio()
{

}

// 递归遍历目录，打印所有文件路径
void list_files(const char *base_path) {
    struct dirent *entry;
    DIR *dp = opendir(base_path);
    
    if (dp == NULL) {
        perror("opendir");
        return;
    }
    
    while ((entry = readdir(dp)) != NULL) {
        // 跳过 . 和 ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // 拼接完整路径
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name);
        
        // 判断是文件还是目录
        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            // 是目录，递归遍历
            list_files(full_path);
        } else {
            // 是文件，打印路径
            printf("%s\n", full_path);
        }
    }
    
    closedir(dp);
}

static void init_sd()
{
    ESP_LOGI(TAG, "init sd");
    esp_err_t ret;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;

    // 3. SPI 总线配置
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_MOSI,
        .miso_io_num = SD_MISO,
        .sclk_io_num = SD_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };

    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
        return;
    }

    // 4. slot（CS）配置
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_CS;
    slot_config.host_id = host.slot;

    // 1. 挂载配置
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;
    ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_PATH, &host, &slot_config,
                                  &mount_config, &card);

    sdmmc_card_print_info(stdout, card);

    // 8. 卸载（实际产品里在掉电前调）
    // esp_vfs_fat_sdcard_unmount(SD_MOUNT_PATH, card);
    // spi_bus_free(host.slot);

    // sd_test_rw();

    //list_files("/sdcard/Music");
}

/* ---------- 2. 播放指定频率、时长的音 ---------- */
static void play_tone(float freq_hz, int duration_ms, float amplitude)
{
    const int total_samples = SAMPLE_RATE * duration_ms / 1000;
    const int chunk = 256;
    /* MONO: 缓冲只要 chunk 个 int16_t */
    int16_t *buf = heap_caps_malloc(chunk * sizeof(int16_t), MALLOC_CAP_DMA);
    if (!buf) return;

    for (int done = 0; done < total_samples; done += chunk) {
        int n = (total_samples - done > chunk) ? chunk : (total_samples - done);
        for (int i = 0; i < n; i++) {
            float t = (float)(done + i) / SAMPLE_RATE;
            buf[i] = (int16_t)(amplitude * sinf(2 * M_PI * freq_hz * t));
        }
        size_t written = 0;
        esp_err_t err = i2s_channel_write(tx_chan, buf, n * sizeof(int16_t), &written, portMAX_DELAY);
        ESP_LOGI(TAG, "err=%d written=%d n=%d", err, written, n);
    }
    free(buf);
}

static void init_speaker(void)
{

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;   // 无数据时自动清 0，避免杂音
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(SLOT_BITS, CHANNEL),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = SPK_BCLK_GPIO,
            .ws   = SPK_WS_GPIO,
            .dout = SPK_DOUT_GPIO,
            .din  = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv   = false,
            },
        },
    };


    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
    ESP_LOGI(TAG, "MAX98357 I2S TX ready: BCLK=5 WS=6 DOUT=4 @ %dHz", SAMPLE_RATE);

    // /* C 大调音名 -> 频率 */
    // const float C5 = 523.25f, D5 = 587.33f, E5 = 659.25f,
    //             F5 = 698.46f, G5 = 783.99f, A5 = 880.00f;

    // /* 振幅，MONO 单声道数据直接缩放 */
    // const float amp = 8000.0f;

    // /* 
    //  * 两只老虎 旋律（单位：ms）
    //  * 1 1 5 5 | 6 6 5 - | 4 4 3 3 | 2 2 1 - 
    //  */
    // play_tone(C5, 400, amp);   // 1
    // play_tone(C5, 400, amp);   // 1
    // play_tone(G5, 400, amp);   // 5
    // play_tone(G5, 400, amp);   // 5

    // play_tone(A5, 400, amp);   // 6
    // play_tone(A5, 400, amp);   // 6
    // play_tone(G5, 800, amp);   // 5 -

    // play_tone(F5, 400, amp);   // 4
    // play_tone(F5, 400, amp);   // 4
    // play_tone(E5, 400, amp);   // 3
    // play_tone(E5, 400, amp);   // 3

    // play_tone(D5, 400, amp);   // 2
    // play_tone(D5, 400, amp);   // 2
    // play_tone(C5, 800, amp);   // 1 -

    // /* 
    //  * 第二遍（也是经典一遍的重复）
    //  * 5 5 4 4 | 3 3 2 - | ...
    //  */
    // play_tone(G5, 400, amp);   // 5
    // play_tone(G5, 400, amp);   // 5
    // play_tone(F5, 400, amp);   // 4
    // play_tone(F5, 400, amp);   // 4

    // play_tone(E5, 400, amp);   // 3
    // play_tone(E5, 400, amp);   // 3
    // play_tone(D5, 800, amp);   // 2 -

    // play_tone(G5, 400, amp);   // 5
    // play_tone(G5, 400, amp);   // 5
    // play_tone(F5, 400, amp);   // 4
    // play_tone(F5, 400, amp);   // 4

    // play_tone(E5, 400, amp);   // 3
    // play_tone(E5, 400, amp);   // 3
    // play_tone(D5, 800, amp);   // 2 -

    // /* 
    //  * “跑得快” 两遍 
    //  * 1 - 5 - | 1 - - -
    //  */
    // play_tone(C5, 400, amp);   // 1
    // play_tone(0.0f, 400, amp); // 空拍
    // play_tone(G5, 400, amp);   // 5
    // play_tone(0.0f, 400, amp); // 空拍

    // play_tone(C5, 800, amp);   // 1 -（结束长音）
}

void periph_manager_init(void)
{
    ESP_LOGI(TAG, "periph_manager_init start");

    // 1. 初始化并挂载 SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_vfs_spiffs_register(&conf);

    // I2C
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t ret = i2c_new_master_bus(&bus_cfg, &g_i2c_bus);

    // SPI LCD
    ESP_LOGI(TAG, "初始化spi_bus");
    spi_bus_config_t bus_config = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LVGL_DRAW_BUF_LINES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));
    
    lcd_init();
    touch_init();

    init_sd();

    // I2S
    init_speaker();

    ESP_LOGI(TAG, "periph_manager_init finish");
}