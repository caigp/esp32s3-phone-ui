#include "globals.h"
#include "esp_log.h"
#include "time.h"
#include "ui/ui.h"
#include "utils/time_format.h"
#include "lv_ui_lock.h"

static const char *TAG = "globals";

QueueHandle_t xGlobalsQueue;
lv_obj_t *toast;
lv_timer_t *toast_timer;

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

static void on_sync_done(const time_sync_result_t *res)
{
    if (res->success) {
        ESP_LOGI(TAG, "Synced — jumped %ld sec via %d",
                 (long)res->jump.delta_sec, res->jump.origin);
    } else {
        ESP_LOGW(TAG, "Sync failed");
    }
    last_time_sync = res->success;
}

static uint64_t timer_count = 0;

static void system_timer_cb(lv_timer_t *timer)
{
    if (ui_statusbar_time)
    {
        time_human_t out;
        time_service_now_human(8 * 3600, &out);
        lv_label_set_text_fmt(ui_statusbar_time, "%02d:%02d", out.hour, out.minute);
    }

    timer_count++;
    if (timer_count % (last_time_sync ? ONE_DAY_SECONDS : ONE_MINUTE_SECONDS) == 0)
    {
        time_sync();
    }
}

void global_init()
{
    //网络时间
    time_init_result_t result;
    time_service_init(&result);

    lv_timer_create(system_timer_cb, 1000, NULL);
}

void time_sync()
{
    time_service_sync_async(on_sync_done);
}

