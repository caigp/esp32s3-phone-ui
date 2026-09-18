#include "globals.h"
#include "esp_log.h"
#include "time.h"
#include "ui/ui.h"
#include "utils/time_format.h"
#include "lv_ui_lock.h"
#include "esp_rmaker_time_sync.h"
#include <time.h>

static const char *TAG = "globals";

QueueHandle_t xGlobalsQueue;
lv_obj_t *toast;
lv_timer_t *toast_timer;

lv_obj_t * ui_notification_panel_ = NULL;
lv_obj_t * ui_Container62_ = NULL;
lv_obj_t * ui_notification_date_label_ = NULL;
lv_obj_t * ui_notification_week_label_ = NULL;

lv_obj_t * ui_statsusbar = NULL;
lv_obj_t * ui_statusbar_time = NULL;
lv_obj_t * ui_statusbar_wifi = NULL;

struct tm timeinfo;

sys_config_t sys_config = {
    .volume = 100,
    .brightness = 128,
};

void set_volume(int volume)
{
    if (volume < 0)
    {
        sys_config.volume -= 10;
        if (sys_config.volume < 0)
        {
            sys_config.volume = 0;
        }
    }
    else if (volume > 100)
    {
        sys_config.volume += 10;
        if (sys_config.volume > 100)
        {
            sys_config.volume = 100;
        }
    }
    else 
    {
        sys_config.volume = volume;

    }

}

void show_toast(char *text)
{
    lv_label_set_text(toast, text);
    lv_obj_remove_flag(toast, LV_OBJ_FLAG_HIDDEN);
    lv_timer_reset(toast_timer);
    lv_timer_resume(toast_timer);
}

static bool last_time_sync = false;

static uint64_t timer_count = 0;

static void _time_sync()
{
    if (esp_rmaker_time_wait_for_sync(pdMS_TO_TICKS(10000)) == ESP_OK)
    {
        last_time_sync = true;
    }
}

static void system_timer_cb(void *args)
{
    while (1)
    {

        time_t now;
        time(&now);

        localtime_r(&now, &timeinfo);
        char buf[64];
        // strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        strftime(buf, sizeof(buf), "%02H:%02M", &timeinfo);
        char date[64];
        strftime(date, sizeof(date), "%Y-%02m-%02d", &timeinfo);

        int weekday = timeinfo.tm_wday;
        const char *days[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};

        if (lv_is_initialized())
        {
            if (ui_statusbar_time && ui_notification_date_label_ && ui_notification_week_label_)
            {
                ui_lock();
                lv_label_set_text_fmt(ui_statusbar_time, "%s", buf);
                lv_label_set_text_fmt(ui_notification_date_label_, "%s", date);
                lv_label_set_text_fmt(ui_notification_week_label_, "%s", days[weekday]);
                ui_unlock();
            }
        }

        timer_count++;
        if (timer_count % (last_time_sync ? ONE_DAY_SECONDS : ONE_MINUTE_SECONDS) == 0)
        {
            _time_sync();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void global_init()
{
    esp_rmaker_time_sync_init(NULL);
    esp_rmaker_time_set_timezone("Asia/Shanghai");
    xTaskCreate(system_timer_cb, "system_timer", 4096, NULL, 5, NULL);
}

void time_sync()
{
    last_time_sync = false;
}

// 1. 定义动画回调函数（适应参数类型）
static void anim_cb_notification_panel(void * var, int32_t v)
{
    lv_obj_set_y((lv_obj_t *)var, v);
}

// 2. 创建并启动动画
void toggle_notification_panel(lv_obj_t * obj, bool show)
{
    lv_anim_t a;
    lv_anim_init(&a);
    
    // 设置作用的目标控件
    lv_anim_set_var(&a, obj);
    
    // 设置回调函数
    lv_anim_set_exec_cb(&a, anim_cb_notification_panel);
    
    if (show)
    {
        lv_anim_set_values(&a, -lv_obj_get_height(obj), 0);
    }
    else
    {
        // 设置起点和终点 y 坐标 (例如从 y=0 移动到 y=200)
        lv_anim_set_values(&a, 0, -lv_obj_get_height(obj));
    }
    
    
    // 设置动画持续时间 (单位: ms)
    lv_anim_set_duration(&a, 200);
    
    // 设置动画缓动曲线 (例如: 弹性效果 lv_anim_path_bounce / 减速 lv_anim_path_ease_out)
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    
    // (可选) 开启往返播放或循环
    // lv_anim_set_playback_duration(&a, 500); // 往返动画时间
    // lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE); // 无限循环
    
    // 启动动画
    lv_anim_start(&a);
}

void ui_event_Notification_Panel(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target_obj(e);
    if (event_code == LV_EVENT_CLICKED)
    {
        toggle_notification_panel(obj, false);
    }
}

void ui_event_top_touch(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target_obj(e);
    if(event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_BOTTOM) {
        lv_indev_wait_release(lv_indev_active());
        toggle_notification_panel(ui_notification_panel_, true);
    }
    else if (event_code == LV_EVENT_CLICKED)
    {
        // toggle_notification_panel(ui_Notification_Panel, true);
    }
}

/* 通知栏初始化 */
void init_notification_panel()
{
    lv_obj_t * obj_t = lv_layer_top();

    lv_obj_t *ui_top_touch = lv_obj_create(obj_t);
    lv_obj_set_size(ui_top_touch, lv_pct(100), 25);
    lv_obj_set_style_bg_color(ui_top_touch, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_top_touch, LV_OPA_0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_top_touch, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ui_top_touch, LV_OBJ_FLAG_GESTURE_BUBBLE);

    ui_notification_panel_ = lv_obj_create(obj_t);
    lv_obj_set_y(ui_notification_panel_, -lv_pct(100));
    lv_obj_set_size(ui_notification_panel_, lv_pct(100), lv_pct(100));
    lv_obj_remove_flag(ui_notification_panel_, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_radius(ui_notification_panel_, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_notification_panel_, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_notification_panel_, 220, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_blur_radius(ui_notification_panel_, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    // lv_obj_set_style_blur_backdrop(ui_notification_panel_, true, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_Container62_ = lv_obj_create(ui_notification_panel_);
    lv_obj_remove_style_all(ui_Container62_);
    lv_obj_set_width(ui_Container62_, LV_SIZE_CONTENT);   /// 100
    lv_obj_set_height(ui_Container62_, LV_SIZE_CONTENT);    /// 50
    lv_obj_set_x(ui_Container62_, 0);
    lv_obj_set_y(ui_Container62_, lv_pct(5));
    lv_obj_set_align(ui_Container62_, LV_ALIGN_TOP_MID);
    lv_obj_set_flex_flow(ui_Container62_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_Container62_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(ui_Container62_, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags

    ui_notification_date_label_ = lv_label_create(ui_Container62_);
    lv_obj_set_width(ui_notification_date_label_, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_notification_date_label_, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_notification_date_label_, -64);
    lv_obj_set_y(ui_notification_date_label_, -137);
    lv_obj_set_align(ui_notification_date_label_, LV_ALIGN_CENTER);
    lv_label_set_text(ui_notification_date_label_, "2026-10-01");
    lv_obj_set_style_text_color(ui_notification_date_label_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_notification_date_label_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_notification_date_label_, &lv_font_montserrat_22, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_notification_week_label_ = lv_label_create(ui_Container62_);
    lv_obj_set_width(ui_notification_week_label_, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_notification_week_label_, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_notification_week_label_, -76);
    lv_obj_set_y(ui_notification_week_label_, -107);
    lv_obj_set_align(ui_notification_week_label_, LV_ALIGN_CENTER);
    lv_label_set_text(ui_notification_week_label_, "星期四");
    lv_obj_set_style_text_color(ui_notification_week_label_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_notification_week_label_, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_notification_week_label_, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_event_cb(ui_notification_panel_, ui_event_Notification_Panel, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(ui_top_touch, ui_event_top_touch, LV_EVENT_ALL, NULL);
}

void init_status_bar()
{
    lv_obj_t * obj_t = lv_layer_top();

    ui_statsusbar = lv_obj_create(obj_t);
    lv_obj_remove_style_all(ui_statsusbar);
    lv_obj_set_height(ui_statsusbar, 25);
    lv_obj_set_width(ui_statsusbar, lv_pct(100));
    lv_obj_remove_flag(ui_statsusbar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(ui_statsusbar, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_statsusbar, LV_OPA_20, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_statusbar_time = lv_label_create(ui_statsusbar);
    lv_obj_set_width(ui_statusbar_time, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_statusbar_time, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_align(ui_statusbar_time, LV_ALIGN_CENTER);
    lv_label_set_text(ui_statusbar_time, "12:00");
    lv_obj_set_style_text_color(ui_statusbar_time, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_opa(ui_statusbar_time, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_statusbar_wifi = lv_image_create(ui_statsusbar);
    lv_image_set_src(ui_statusbar_wifi, &ui_img_icon_wifi_small_png);
    lv_obj_set_width(ui_statusbar_wifi, LV_SIZE_CONTENT);   /// 1
    lv_obj_set_height(ui_statusbar_wifi, LV_SIZE_CONTENT);    /// 1
    lv_obj_set_x(ui_statusbar_wifi, 10);
    lv_obj_set_y(ui_statusbar_wifi, 0);
    lv_obj_set_align(ui_statusbar_wifi, LV_ALIGN_LEFT_MID);
    lv_obj_add_flag(ui_statusbar_wifi, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_CLICKABLE);     /// Flags
    lv_obj_remove_flag(ui_statusbar_wifi, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_image_set_inner_align(ui_statusbar_wifi, LV_IMAGE_ALIGN_STRETCH); 
}

void ui_event_navigation_bar(lv_event_t * e)
{ 

    lv_event_code_t event_code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target_obj(e);
    if(event_code == LV_EVENT_GESTURE &&  lv_indev_get_gesture_dir(lv_indev_active()) == LV_DIR_TOP) {
        lv_indev_wait_release(lv_indev_active());
        _ui_screen_change(&ui_launcher, LV_SCR_LOAD_ANIM_OUT_TOP, 300, 0, &ui_launcher_screen_init);
    }
    // else if (event_code == LV_EVENT_CLICKED)
    // {
    //     _ui_screen_change(&ui_launcher, LV_SCR_LOAD_ANIM_OUT_TOP, 300, 0, &ui_launcher_screen_init);
    // }
    
}

/**
    底部小白条
 */
void init_navigation_bar()
{
    // 获取顶层图层作为导航栏的父容器
    lv_obj_t * obj_t = lv_layer_top();

    // 创建导航栏主容器并设置基本属性
    lv_obj_t * ui_navigation_bar = lv_obj_create(obj_t);
    lv_obj_remove_style_all(ui_navigation_bar);
    lv_obj_set_height(ui_navigation_bar, 25);
    lv_obj_set_width(ui_navigation_bar, lv_pct(100));
    lv_obj_set_align(ui_navigation_bar, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_style_bg_color(ui_navigation_bar, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_navigation_bar, LV_OPA_0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(ui_navigation_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(ui_navigation_bar, LV_OBJ_FLAG_GESTURE_BUBBLE);

    // 创建白条指示器，使用差值混合模式实现反色效果
    lv_obj_t * ui_baitiao = lv_obj_create(ui_navigation_bar);
    lv_obj_remove_style_all(ui_baitiao);
    lv_obj_set_size(ui_baitiao, lv_pct(50), 5);
    lv_obj_set_style_blend_mode(ui_baitiao, LV_BLEND_MODE_DIFFERENCE, 0);
    lv_obj_set_style_bg_color(ui_baitiao, lv_color_hex(0xffffff), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_baitiao, LV_OPA_50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_baitiao, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_align(ui_baitiao, LV_ALIGN_CENTER);

    // 注册导航栏事件回调函数
    lv_obj_add_event_cb(ui_navigation_bar, ui_event_navigation_bar, LV_EVENT_ALL, NULL);
}
