file(GLOB_RECURSE SRC_UI "ui/*.c")
file(GLOB_RECURSE SRC_UTILS 
    "utils/*.c"
    "utils/*.cpp"
)
file(GLOB_RECURSE SRC_HARDWARE "hardware/*.c")
file(GLOB_RECURSE SRC_INPUT "input/*.c")

idf_component_register(SRCS "media_init.c" "periph_manager.c" "main.c" ${SRC_UI} "ui_event_impl.c"
                    ${SRC_UTILS} ${SRC_HARDWARE} ${SRC_INPUT} "player_wrapper.c" "globals.c" "lv_ui_lock.c" "lvgl_demo_ui.c"
                    "wifi.c"
                    INCLUDE_DIRS "." "ui" "hardware" "utils" "input"
                    REQUIRES esp_driver_i2c esp_timer driver sdmmc fatfs 
                    vfs esp_common esp_audio_render esp_codec_dev nofrendo
                    nofrendo-esp32 esp_wifi nvs_flash esp_lcd_touch_ft6x36 spiffs)
