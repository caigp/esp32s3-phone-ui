#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "wifi.h"
#include "esp_mac.h"

extern QueueHandle_t xGlobalsQueue;

extern lv_obj_t *toast;
extern lv_timer_t *toast_timer;
extern TaskHandle_t wifi_scan_task;

extern lv_obj_t * ui_statsusbar;
extern lv_obj_t * ui_statusbar_time;
extern lv_obj_t * ui_statusbar_wifi;

#define VOLUME_DOWM         (0)
#define VOLUME_UP           (1)

#define ONE_DAY_SECONDS     (86400)
#define ONE_MINUTE_SECONDS  (60)

// 全局可变参数结构体
typedef struct {
    int   volume;          // 当前音量
    unsigned char brightness;    // 当前亮度
} sys_config_t;

extern sys_config_t sys_config;   // 外部声明全局实例

extern struct tm timeinfo;

extern void set_volume(int volume);

extern void show_toast(char *text);

extern void global_init();
extern void time_sync();

extern void init_status_bar();
extern void init_notification_panel();
extern void init_navigation_bar();