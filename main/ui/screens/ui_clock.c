#include "ui_clock.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_ALARM_COUNT 20
#define MAX_WORLD_CLOCK_COUNT 20
#define MAX_STOPWATCH_LAPS 50

// 外部字体声明
LV_FONT_DECLARE(lv_font_montserrat_22);

///////////////////// STRUCTURES & TYPES //////////////////

// 世界时钟数据结构
typedef struct {
    int id;
    char city_name[32];
    int tz_offset_hours; // 相对于 UTC 的时区偏移
} world_clock_item_t;

// 预定义城市库（供添加弹窗选择）
typedef struct {
    const char * city_name;
    int tz_offset_hours;
} city_info_t;

static const city_info_t g_preset_cities[] = {
    {"北京", 8},
    {"东京", 9},
    {"悉尼", 10},
    {"伦敦", 0},
    {"巴黎", 1},
    {"纽约", -5},
    {"旧金山", -8},
    {"迪拜", 4}
};
static const size_t g_preset_city_count = sizeof(g_preset_cities) / sizeof(g_preset_cities[0]);

///////////////////// VARIABLES //////////////////
lv_obj_t * ui_clock = NULL;
lv_obj_t * ui_TopBarPlaceholder = NULL;
lv_obj_t * ui_ContentArea = NULL;
lv_obj_t * ui_TitleLabel = NULL;
lv_obj_t * ui_SubTitleLabel = NULL;
lv_obj_t * ui_TabBar = NULL;
lv_obj_t * ui_TabBtnAlarm = NULL;
lv_obj_t * ui_TabBtnWorld = NULL;
lv_obj_t * ui_TabBtnStopwatch = NULL;
lv_obj_t * ui_TabBtnTimer = NULL;

// 静态字符串缓冲区
static char g_alarm_time_bufs[MAX_ALARM_COUNT][16];

// 闹钟列表数据
static alarm_item_t g_alarm_list[MAX_ALARM_COUNT] = {
    {1, 6, 46, "06:46", ALARM_REPEAT_EVERYDAY, "起床", true},
    {2, 15, 57, "15:57", ALARM_REPEAT_MON | ALARM_REPEAT_TUE | ALARM_REPEAT_WED | ALARM_REPEAT_THU | ALARM_REPEAT_FRI, "工作提醒", true},
    {3, 16, 31, "16:31", ALARM_REPEAT_SAT | ALARM_REPEAT_SUN, "休息", false},
    {4, 17, 3, "17:03", 0, "看电影", false}
};
static size_t g_alarm_count = 4;

// 闹钟对话框 UI 句柄及暂存状态
static lv_obj_t * ui_AlarmDialog = NULL;
static lv_obj_t * ui_RollerHour = NULL;
static lv_obj_t * ui_RollerMin = NULL;
static uint8_t g_selected_repeat_days = ALARM_REPEAT_EVERYDAY;
static alarm_item_t * g_editing_alarm = NULL;

// 世界时钟动态数据
static world_clock_item_t g_world_clock_list[MAX_WORLD_CLOCK_COUNT] = {
    {1, "北京", 8},
    {2, "伦敦", 0},
    {3, "纽约", -5}
};
static size_t g_world_clock_count = 3;

// 世界时钟通用弹窗与定时器句柄
static lv_obj_t * ui_WorldDialog = NULL;
static lv_timer_t * g_world_clock_timer = NULL; // 实时刷新定时器
static bool g_is_world_clock_active = false;    // 标记当前是否处于世界时钟子页面

// ================= 秒表模块数据与句柄 =================
typedef enum {
    STOPWATCH_STATE_STOPPED,
    STOPWATCH_STATE_RUNNING,
    STOPWATCH_STATE_PAUSED
} stopwatch_state_t;

static stopwatch_state_t g_stopwatch_state = STOPWATCH_STATE_STOPPED;
static uint32_t g_stopwatch_ms = 0;              // 耗时毫秒
static lv_timer_t * g_stopwatch_timer = NULL;     // 秒表定时器 (30ms周期)
static uint32_t g_stopwatch_laps[MAX_STOPWATCH_LAPS];
static size_t g_stopwatch_lap_count = 0;

static lv_obj_t * ui_StopwatchTimeLabel = NULL;
static lv_obj_t * ui_StopwatchBtnStartPause = NULL;
static lv_obj_t * ui_StopwatchBtnStartPauseLbl = NULL;
static lv_obj_t * ui_StopwatchBtnLapReset = NULL;
static lv_obj_t * ui_StopwatchBtnLapResetLbl = NULL;
static lv_obj_t * ui_StopwatchLapListCnt = NULL;

// ================= 计时器模块数据与句柄 =================
typedef enum {
    TIMER_STATE_SETTING,
    TIMER_STATE_RUNNING,
    TIMER_STATE_PAUSED
} timer_state_t;

static timer_state_t g_timer_state = TIMER_STATE_SETTING;
static uint32_t g_timer_total_sec = 900;          // 设置的总秒数（默认15分）
static uint32_t g_timer_remaining_sec = 900;      // 剩余秒数
static lv_timer_t * g_timer_timer = NULL;          // 倒计时定时器 (1秒周期)

static lv_obj_t * ui_TimerDisplayCnt = NULL;
static lv_obj_t * ui_TimerTimeLabel = NULL;
static lv_obj_t * ui_TimerRollerCnt = NULL;
static lv_obj_t * ui_TimerRollerH = NULL;
static lv_obj_t * ui_TimerRollerM = NULL;
static lv_obj_t * ui_TimerRollerS = NULL;
static lv_obj_t * ui_TimerBtnStartPause = NULL;
static lv_obj_t * ui_TimerBtnStartPauseLbl = NULL;
static lv_obj_t * ui_TimerBtnCancel = NULL;
static lv_obj_t * ui_TimerAlertDialog = NULL;

// 函数前置声明
static void ui_build_alarm_list(lv_obj_t * parent);
static void ui_build_world_clock(lv_obj_t * parent);
static void ui_build_stopwatch(lv_obj_t * parent);
static void ui_build_timer(lv_obj_t * parent);

static void ui_open_alarm_dialog(alarm_item_t * item);
static void ui_open_add_world_clock_dialog(void);
static void ui_start_world_clock_timer(void);
static void ui_stop_world_clock_timer(void);

static void ui_stopwatch_cleanup(void);
static void ui_timer_cleanup(void);

///////////////////// HELPER FUNCTIONS //////////////////

static void ui_get_repeat_days_str(uint8_t repeat_days, char * buf, size_t buf_len)
{
    if (repeat_days == ALARM_REPEAT_EVERYDAY) {
        snprintf(buf, buf_len, "每天");
        return;
    }
    if (repeat_days == 0) {
        snprintf(buf, buf_len, "响一次");
        return;
    }

    const char * week_names[] = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
    buf[0] = '\0';
    bool first = true;

    int order[] = {1, 2, 3, 4, 5, 6, 0};
    for (int i = 0; i < 7; i++) {
        int day = order[i];
        if (repeat_days & (1 << day)) {
            if (!first) {
                snprintf(buf + strlen(buf), buf_len - strlen(buf), "、");
            }
            snprintf(buf + strlen(buf), buf_len - strlen(buf), "%s", week_names[day]);
            first = false;
        }
    }
}

// 帮助函数：计算指定 UTC 偏移的时区时间和相比本地的时差字符串
static void ui_calc_world_clock_data(int target_tz_offset, char * time_buf, size_t time_len, char * diff_buf, size_t diff_len)
{
    time_t now = time(NULL);
    struct tm * utc_tm = gmtime(&now);
    struct tm * local_tm = localtime(&now);

    if (!utc_tm || !local_tm) {
        snprintf(time_buf, time_len, "--:--");
        snprintf(diff_buf, diff_len, "未知");
        return;
    }

    // 计算本地相对于 UTC 的偏移小时数
    time_t local_sec = mktime(local_tm);
    struct tm utc_as_local = *utc_tm;
    time_t utc_sec = mktime(&utc_as_local);
    int local_tz_offset = (int)difftime(local_sec, utc_sec) / 3600;

    // 计算目标时区时间
    time_t target_sec = now + (target_tz_offset * 3600);
    struct tm * target_tm = gmtime(&target_sec);

    if (target_tm) {
        snprintf(time_buf, time_len, "%02d:%02d", target_tm->tm_hour, target_tm->tm_min);
    } else {
        snprintf(time_buf, time_len, "--:--");
    }

    // 计算与本地的时差
    int diff_hours = target_tz_offset - local_tz_offset;
    if (diff_hours == 0) {
        snprintf(diff_buf, diff_len, "本地时间");
    } else if (diff_hours > 0) {
        snprintf(diff_buf, diff_len, "较本地快 %d 小时", diff_hours);
    } else {
        snprintf(diff_buf, diff_len, "较本地慢 %d 小时", -diff_hours);
    }
}

///////////////////// EXTERNAL INTERFACE STUBS //////////////////

const char * ui_get_next_alarm_remaining_str(void)
{
    static char remaining_buf[128];

    time_t now_sec = time(NULL);
    struct tm * now_tm = localtime(&now_sec);

    if (now_tm == NULL) {
        return "无法获取系统时间";
    }

    time_t min_target_sec = 0;
    bool found_active = false;

    for (size_t i = 0; i < g_alarm_count; i++) {
        if (!g_alarm_list[i].is_enabled) continue;

        for (int day_offset = 0; day_offset < 7; day_offset++) {
            struct tm target_tm = *now_tm;
            target_tm.tm_mday += day_offset;
            target_tm.tm_hour = g_alarm_list[i].hour;
            target_tm.tm_min = g_alarm_list[i].minute;
            target_tm.tm_sec = 0;

            time_t target_sec = mktime(&target_tm);

            if (target_sec <= now_sec) {
                continue;
            }

            struct tm * calc_tm = localtime(&target_sec);
            int week_day = calc_tm->tm_wday;

            if (g_alarm_list[i].repeat_days == 0 || (g_alarm_list[i].repeat_days & (1 << week_day))) {
                if (!found_active || target_sec < min_target_sec) {
                    min_target_sec = target_sec;
                    found_active = true;
                }
                break;
            }
        }
    }

    if (!found_active) {
        return "所有闹钟已关闭";
    }

    long diff_sec = (long)(min_target_sec - now_sec);
    int diff_days = diff_sec / 86400;
    int diff_hours = (diff_sec % 86400) / 3600;
    int diff_minutes = (diff_sec % 3600) / 60;

    if (diff_days > 0) {
        if (diff_hours > 0) {
            snprintf(remaining_buf, sizeof(remaining_buf), "%d天%d小时后响铃", diff_days, diff_hours);
        } else {
            snprintf(remaining_buf, sizeof(remaining_buf), "%d天后响铃", diff_days);
        }
    } else if (diff_hours > 0) {
        snprintf(remaining_buf, sizeof(remaining_buf), "%d小时%d分钟后响铃", diff_hours, diff_minutes);
    } else {
        snprintf(remaining_buf, sizeof(remaining_buf), "%d分钟后响铃", diff_minutes);
    }

    return remaining_buf;
}

void ui_update_alarm_subtitle(void)
{
    if (ui_SubTitleLabel == NULL) return;

    bool any_active = false;
    for (size_t i = 0; i < g_alarm_count; i++) {
        if (g_alarm_list[i].is_enabled) {
            any_active = true;
            break;
        }
    }

    if (any_active) {
        const char * remaining_str = ui_get_next_alarm_remaining_str();
        lv_label_set_text(ui_SubTitleLabel, remaining_str);
    } else {
        lv_label_set_text(ui_SubTitleLabel, "所有闹钟已关闭");
    }
}

///////////////////// WORLD CLOCK REFRESH TIMER //////////////////

// 世界时钟秒级刷新回调
static void ui_world_clock_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if (ui_ContentArea == NULL) return;

    uint32_t child_cnt = lv_obj_get_child_cnt(ui_ContentArea);
    for (uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t * child = lv_obj_get_child(ui_ContentArea, i);
        world_clock_item_t * item = (world_clock_item_t *)lv_obj_get_user_data(child);

        if (item != NULL) {
            char time_buf[16];
            char diff_buf[64];
            ui_calc_world_clock_data(item->tz_offset_hours, time_buf, sizeof(time_buf), diff_buf, sizeof(diff_buf));

            // text_cnt 是第一个子节点
            lv_obj_t * text_cnt = lv_obj_get_child(child, 0);
            if (text_cnt) {
                lv_obj_t * lbl_diff = lv_obj_get_child(text_cnt, 1);
                if (lbl_diff) lv_label_set_text(lbl_diff, diff_buf);
            }

            // lbl_time 是第二个子节点
            lv_obj_t * lbl_time = lv_obj_get_child(child, 1);
            if (lbl_time) lv_label_set_text(lbl_time, time_buf);
        }
    }
}

// 启动世界时钟刷新定时器
static void ui_start_world_clock_timer(void)
{
    if (g_is_world_clock_active && g_world_clock_timer == NULL) {
        g_world_clock_timer = lv_timer_create(ui_world_clock_timer_cb, 1000, NULL);
    }
}

// 停止世界时钟刷新定时器
static void ui_stop_world_clock_timer(void)
{
    if (g_world_clock_timer != NULL) {
        lv_timer_del(g_world_clock_timer);
        g_world_clock_timer = NULL;
    }
}

// 自动响应屏幕加载/卸载生命周期事件
static void ui_clock_screen_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_SCREEN_LOADED) {
        ui_start_world_clock_timer();
    } else if (code == LV_EVENT_SCREEN_UNLOADED) {
        ui_stop_world_clock_timer();
    }
}

///////////////////// EVENT CALLBACKS //////////////////

void ui_event_AlarmSwitch_toggle(lv_event_t * e)
{
    lv_obj_t * sw = lv_event_get_target(e);
    alarm_item_t * item = (alarm_item_t *)lv_event_get_user_data(e);

    if (item != NULL) {
        item->is_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    }

    ui_update_alarm_subtitle();
}

void ui_event_AlarmCard_edit(lv_event_t * e)
{
    alarm_item_t * item = (alarm_item_t *)lv_event_get_user_data(e);
    if (item != NULL) {
        ui_open_alarm_dialog(item);
    }
}

static void ui_event_WeekBtn_click(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target(e);
    uint8_t day_mask = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    if (lv_obj_has_state(btn, LV_STATE_CHECKED)) {
        g_selected_repeat_days |= day_mask;
    } else {
        g_selected_repeat_days &= ~day_mask;
    }
}

static void ui_event_AlarmDialog_cancel(lv_event_t * e)
{
    LV_UNUSED(e);
    if (ui_AlarmDialog != NULL) {
        lv_obj_del(ui_AlarmDialog);
        ui_AlarmDialog = NULL;
    }
}

static void ui_event_AlarmDialog_delete(lv_event_t * e)
{
    LV_UNUSED(e);
    if (g_editing_alarm != NULL) {
        int index = -1;
        for (size_t i = 0; i < g_alarm_count; i++) {
            if (&g_alarm_list[i] == g_editing_alarm) {
                index = (int)i;
                break;
            }
        }

        if (index >= 0) {
            for (size_t i = index; i < g_alarm_count - 1; i++) {
                g_alarm_list[i] = g_alarm_list[i + 1];
            }
            g_alarm_count--;
        }
    }

    if (ui_AlarmDialog != NULL) {
        lv_obj_del(ui_AlarmDialog);
        ui_AlarmDialog = NULL;
    }

    if (ui_ContentArea != NULL) {
        lv_obj_clean(ui_ContentArea);
        ui_build_alarm_list(ui_ContentArea);
    }
}

static void ui_event_AlarmDialog_save(lv_event_t * e)
{
    LV_UNUSED(e);

    uint16_t selected_hour = lv_roller_get_selected(ui_RollerHour);
    uint16_t selected_min = lv_roller_get_selected(ui_RollerMin);

    if (g_editing_alarm != NULL) {
        int idx = (int)(g_editing_alarm - g_alarm_list);
        if (idx >= 0 && idx < MAX_ALARM_COUNT) {
            snprintf(g_alarm_time_bufs[idx], sizeof(g_alarm_time_bufs[idx]), "%02d:%02d", selected_hour, selected_min);
            g_editing_alarm->hour = (uint8_t)selected_hour;
            g_editing_alarm->minute = (uint8_t)selected_min;
            g_editing_alarm->time_str = g_alarm_time_bufs[idx];
            g_editing_alarm->repeat_days = g_selected_repeat_days;
        }
    } else {
        if (g_alarm_count >= MAX_ALARM_COUNT) {
            LV_LOG_WARN("闹钟列表已满！");
            if (ui_AlarmDialog) {
                lv_obj_del(ui_AlarmDialog);
                ui_AlarmDialog = NULL;
            }
            return;
        }

        snprintf(g_alarm_time_bufs[g_alarm_count], sizeof(g_alarm_time_bufs[g_alarm_count]), "%02d:%02d", selected_hour, selected_min);

        alarm_item_t new_alarm;
        memset(&new_alarm, 0, sizeof(alarm_item_t));
        new_alarm.id = (int)(g_alarm_count + 1);
        new_alarm.hour = (uint8_t)selected_hour;
        new_alarm.minute = (uint8_t)selected_min;
        new_alarm.time_str = g_alarm_time_bufs[g_alarm_count];
        new_alarm.repeat_days = g_selected_repeat_days;
        new_alarm.tag_str = "闹钟";
        new_alarm.is_enabled = true;

        g_alarm_list[g_alarm_count] = new_alarm;
        g_alarm_count++;
    }

    if (ui_AlarmDialog != NULL) {
        lv_obj_del(ui_AlarmDialog);
        ui_AlarmDialog = NULL;
    }

    if (ui_ContentArea != NULL) {
        lv_obj_clean(ui_ContentArea);
        ui_build_alarm_list(ui_ContentArea);
    }
}

void ui_event_AddAlarm_click(lv_event_t * e)
{
    LV_UNUSED(e);
    ui_open_alarm_dialog(NULL);
}

///////////////////// WORLD CLOCK EVENTS & DIALOGS //////////////////

static void ui_event_WorldDialog_close(lv_event_t * e)
{
    LV_UNUSED(e);
    if (ui_WorldDialog != NULL) {
        lv_obj_del(ui_WorldDialog);
        ui_WorldDialog = NULL;
    }
}

static void ui_event_DeleteWorldClock_confirm(lv_event_t * e)
{
    world_clock_item_t * item = (world_clock_item_t *)lv_event_get_user_data(e);
    if (item != NULL) {
        int index = -1;
        for (size_t i = 0; i < g_world_clock_count; i++) {
            if (&g_world_clock_list[i] == item) {
                index = (int)i;
                break;
            }
        }

        if (index >= 0) {
            for (size_t i = index; i < g_world_clock_count - 1; i++) {
                g_world_clock_list[i] = g_world_clock_list[i + 1];
            }
            g_world_clock_count--;
        }
    }

    ui_event_WorldDialog_close(NULL);

    if (ui_ContentArea != NULL) {
        lv_obj_clean(ui_ContentArea);
        ui_build_world_clock(ui_ContentArea);
    }
}

static void ui_event_WorldClockCard_click(lv_event_t * e)
{
    if (lv_indev_get_scroll_obj(lv_indev_get_act()) != NULL) return;

    world_clock_item_t * item = (world_clock_item_t *)lv_event_get_user_data(e);
    if (item == NULL || ui_WorldDialog != NULL) return;

    ui_WorldDialog = lv_obj_create(lv_scr_act());
    lv_obj_add_flag(ui_WorldDialog, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(ui_WorldDialog, lv_pct(100), lv_pct(100));
    lv_obj_align(ui_WorldDialog, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(ui_WorldDialog, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ui_WorldDialog, 160, 0);
    lv_obj_set_style_border_width(ui_WorldDialog, 0, 0);
    lv_obj_set_style_pad_all(ui_WorldDialog, 0, 0);
    lv_obj_clear_flag(ui_WorldDialog, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * card = lv_obj_create(ui_WorldDialog);
    lv_obj_set_width(card, lv_pct(80));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_center(card);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(card, 20, 0);
    lv_obj_set_style_border_width(card, 0, 0);

    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * title = lv_label_create(card);
    char msg[64];
    snprintf(msg, sizeof(msg), "是否删除“%s”？", item->city_name);
    lv_label_set_text(title, msg);
    lv_obj_set_style_text_font(title, &ui_font_simhei14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x111111), 0);
    lv_obj_set_style_pad_bottom(title, 16, 0);

    lv_obj_t * btn_cnt = lv_obj_create(card);
    lv_obj_set_width(btn_cnt, lv_pct(100));
    lv_obj_set_height(btn_cnt, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_cnt, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cnt, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_cnt, 0, 0);
    lv_obj_set_style_border_width(btn_cnt, 0, 0);
    lv_obj_set_style_pad_all(btn_cnt, 0, 0);

    lv_obj_t * btn_cancel = lv_btn_create(btn_cnt);
    lv_obj_set_width(btn_cancel, lv_pct(46));
    lv_obj_set_height(btn_cancel, 38);
    lv_obj_set_style_radius(btn_cancel, 19, 0);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0xE5E5EA), 0);
    lv_obj_set_style_shadow_width(btn_cancel, 0, 0);
    lv_obj_add_event_cb(btn_cancel, ui_event_WorldDialog_close, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "取消");
    lv_obj_set_style_text_font(lbl_cancel, &ui_font_simhei14, 0);
    lv_obj_set_style_text_color(lbl_cancel, lv_color_hex(0x333333), 0);
    lv_obj_center(lbl_cancel);

    lv_obj_t * btn_del = lv_btn_create(btn_cnt);
    lv_obj_set_width(btn_del, lv_pct(46));
    lv_obj_set_height(btn_del, 38);
    lv_obj_set_style_radius(btn_del, 19, 0);
    lv_obj_set_style_bg_color(btn_del, lv_color_hex(0xFF3B30), 0);
    lv_obj_set_style_shadow_width(btn_del, 0, 0);
    lv_obj_add_event_cb(btn_del, ui_event_DeleteWorldClock_confirm, LV_EVENT_CLICKED, item);

    lv_obj_t * lbl_del = lv_label_create(btn_del);
    lv_label_set_text(lbl_del, "删除");
    lv_obj_set_style_text_font(lbl_del, &ui_font_simhei14, 0);
    lv_obj_set_style_text_color(lbl_del, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_del);
}

static void ui_event_SelectCity_click(lv_event_t * e)
{
    const city_info_t * city = (const city_info_t *)lv_event_get_user_data(e);
    if (city != NULL) {
        if (g_world_clock_count >= MAX_WORLD_CLOCK_COUNT) {
            LV_LOG_WARN("世界时钟列表已满");
            ui_event_WorldDialog_close(NULL);
            return;
        }

        for (size_t i = 0; i < g_world_clock_count; i++) {
            if (strcmp(g_world_clock_list[i].city_name, city->city_name) == 0) {
                ui_event_WorldDialog_close(NULL);
                return;
            }
        }

        world_clock_item_t new_item;
        new_item.id = (int)(g_world_clock_count + 1);
        strncpy(new_item.city_name, city->city_name, sizeof(new_item.city_name) - 1);
        new_item.tz_offset_hours = city->tz_offset_hours;

        g_world_clock_list[g_world_clock_count] = new_item;
        g_world_clock_count++;
    }

    ui_event_WorldDialog_close(NULL);

    if (ui_ContentArea != NULL) {
        lv_obj_clean(ui_ContentArea);
        ui_build_world_clock(ui_ContentArea);
    }
}

static void ui_open_add_world_clock_dialog(void)
{
    if (ui_WorldDialog != NULL) return;

    ui_WorldDialog = lv_obj_create(lv_scr_act());
    lv_obj_add_flag(ui_WorldDialog, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(ui_WorldDialog, lv_pct(100), lv_pct(100));
    lv_obj_align(ui_WorldDialog, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(ui_WorldDialog, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ui_WorldDialog, 160, 0);
    lv_obj_set_style_border_width(ui_WorldDialog, 0, 0);
    lv_obj_set_style_pad_all(ui_WorldDialog, 0, 0);
    lv_obj_clear_flag(ui_WorldDialog, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * card = lv_obj_create(ui_WorldDialog);
    lv_obj_set_width(card, lv_pct(85));
    lv_obj_set_height(card, lv_pct(70));
    lv_obj_center(card);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_style_border_width(card, 0, 0);

    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);

    lv_obj_t * header = lv_obj_create(card);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "选择城市");
    lv_obj_set_style_text_font(title, &ui_font_simhei14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x111111), 0);

    lv_obj_t * close_btn = lv_btn_create(header);
    lv_obj_set_size(close_btn, 30, 30);
    lv_obj_set_style_radius(close_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xE5E5EA), 0);
    lv_obj_set_style_shadow_width(close_btn, 0, 0);
    lv_obj_add_event_cb(close_btn, ui_event_WorldDialog_close, LV_EVENT_CLICKED, NULL);

    lv_obj_t * close_lbl = lv_label_create(close_btn);
    lv_label_set_text(close_lbl, "X");
    lv_obj_set_style_text_font(close_lbl, &ui_font_simhei14, 0);
    lv_obj_set_style_text_color(close_lbl, lv_color_hex(0x666666), 0);
    lv_obj_center(close_lbl);

    lv_obj_t * list_cnt = lv_obj_create(card);
    lv_obj_set_width(list_cnt, lv_pct(100));
    lv_obj_set_flex_grow(list_cnt, 1);
    lv_obj_set_flex_flow(list_cnt, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(list_cnt, 0, 0);
    lv_obj_set_style_border_width(list_cnt, 0, 0);
    lv_obj_set_style_pad_all(list_cnt, 0, 0);
    lv_obj_set_style_pad_row(list_cnt, 6, 0);

    for (size_t i = 0; i < g_preset_city_count; i++) {
        lv_obj_t * item_btn = lv_btn_create(list_cnt);
        lv_obj_set_width(item_btn, lv_pct(100));
        lv_obj_set_height(item_btn, 40);
        lv_obj_set_style_bg_color(item_btn, lv_color_hex(0xF8F9FA), 0);
        lv_obj_set_style_radius(item_btn, 8, 0);
        lv_obj_set_style_shadow_width(item_btn, 0, 0);
        lv_obj_add_event_cb(item_btn, ui_event_SelectCity_click, LV_EVENT_CLICKED, (void*)&g_preset_cities[i]);

        lv_obj_t * name_lbl = lv_label_create(item_btn);
        lv_label_set_text(name_lbl, g_preset_cities[i].city_name);
        lv_obj_set_style_text_font(name_lbl, &ui_font_simhei14, 0);
        lv_obj_set_style_text_color(name_lbl, lv_color_hex(0x222222), 0);
        lv_obj_align(name_lbl, LV_ALIGN_LEFT_MID, 10, 0);
    }
}

static void ui_event_AddWorldClock_click(lv_event_t * e)
{
    LV_UNUSED(e);
    ui_open_add_world_clock_dialog();
}

///////////////////// DIALOG CREATION (ALARM) //////////////////

static void ui_open_alarm_dialog(alarm_item_t * item)
{
    if (ui_AlarmDialog != NULL) return;

    g_editing_alarm = item;

    int current_hour = 8;
    int current_min = 0;

    if (item != NULL) {
        current_hour = item->hour;
        current_min = item->minute;
        g_selected_repeat_days = item->repeat_days;
    } else {
        time_t now_sec = time(NULL);
        struct tm * now_tm = localtime(&now_sec);
        if (now_tm != NULL) {
            current_hour = now_tm->tm_hour;
            current_min = now_tm->tm_min;
        }
        g_selected_repeat_days = ALARM_REPEAT_EVERYDAY;
    }

    ui_AlarmDialog = lv_obj_create(lv_scr_act());
    lv_obj_add_flag(ui_AlarmDialog, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(ui_AlarmDialog, lv_pct(100), lv_pct(100));
    lv_obj_align(ui_AlarmDialog, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(ui_AlarmDialog, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ui_AlarmDialog, 160, 0);
    lv_obj_set_style_border_width(ui_AlarmDialog, 0, 0);
    lv_obj_set_style_pad_all(ui_AlarmDialog, 0, 0);
    lv_obj_clear_flag(ui_AlarmDialog, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_move_foreground(ui_AlarmDialog);

    lv_obj_t * card = lv_obj_create(ui_AlarmDialog);
    lv_obj_set_width(card, lv_pct(90));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_center(card);
    
    lv_obj_set_style_radius(card, 20, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(card, 16, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * title = lv_label_create(card);
    lv_label_set_text(title, (item != NULL) ? "修改闹钟" : "添加闹钟");
    lv_obj_set_style_text_font(title, &ui_font_simhei14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x111111), 0);
    lv_obj_set_style_pad_bottom(title, 8, 0);

    lv_obj_t * roller_cnt = lv_obj_create(card);
    lv_obj_set_width(roller_cnt, lv_pct(100));
    lv_obj_set_height(roller_cnt, LV_SIZE_CONTENT); 
    lv_obj_set_flex_flow(roller_cnt, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(roller_cnt, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(roller_cnt, 0, 0);
    lv_obj_set_style_border_width(roller_cnt, 0, 0);
    lv_obj_set_style_pad_all(roller_cnt, 0, 0);

    char hour_options[128] = "";
    for (int i = 0; i < 24; i++) {
        snprintf(hour_options + strlen(hour_options), sizeof(hour_options) - strlen(hour_options), "%02d\n", i);
    }
    hour_options[strlen(hour_options) - 1] = '\0';

    char min_options[320] = "";
    for (int i = 0; i < 60; i++) {
        snprintf(min_options + strlen(min_options), sizeof(min_options) - strlen(min_options), "%02d\n", i);
    }
    min_options[strlen(min_options) - 1] = '\0';

    static lv_style_t style_roller_main;
    static lv_style_t style_roller_selected;
    static bool inited = false;
    if(!inited) {
        lv_style_init(&style_roller_main);
        lv_style_set_bg_color(&style_roller_main, lv_color_hex(0xf3f4f6));
        lv_style_set_bg_opa(&style_roller_main, 255);
        lv_style_set_radius(&style_roller_main, 12);
        lv_style_set_border_color(&style_roller_main, lv_color_hex(0xd1d5db));
        lv_style_set_border_width(&style_roller_main, 1);
        lv_style_set_text_color(&style_roller_main, lv_color_hex(0x6b7280));

        lv_style_init(&style_roller_selected);
        lv_style_set_bg_color(&style_roller_selected, lv_color_hex(0x6366f1));
        lv_style_set_bg_opa(&style_roller_selected, 255);
        lv_style_set_text_color(&style_roller_selected, lv_color_hex(0xffffff));
        inited = true;
    }

    ui_RollerHour = lv_roller_create(roller_cnt);
    lv_roller_set_options(ui_RollerHour, hour_options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(ui_RollerHour, 5);
    lv_obj_set_style_text_align(ui_RollerHour, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(ui_RollerHour, lv_pct(40));
    lv_obj_set_style_text_font(ui_RollerHour, &ui_font_simhei14, 0);
    lv_obj_set_style_text_font(ui_RollerHour, &ui_font_simhei14, LV_PART_SELECTED);
    lv_obj_add_style(ui_RollerHour, &style_roller_main, LV_PART_MAIN);
    lv_obj_add_style(ui_RollerHour, &style_roller_selected, LV_PART_SELECTED);

    lv_obj_t * colon = lv_label_create(roller_cnt);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_text_font(colon, &ui_font_simhei14, 0);
    lv_obj_set_style_pad_left(colon, 8, 0);
    lv_obj_set_style_pad_right(colon, 8, 0);

    ui_RollerMin = lv_roller_create(roller_cnt);
    lv_roller_set_options(ui_RollerMin, min_options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(ui_RollerMin, 5);
    lv_obj_set_style_text_align(ui_RollerMin, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(ui_RollerMin, lv_pct(40));
    lv_obj_set_style_text_font(ui_RollerMin, &ui_font_simhei14, 0);
    lv_obj_set_style_text_font(ui_RollerMin, &ui_font_simhei14, LV_PART_SELECTED);
    lv_obj_add_style(ui_RollerMin, &style_roller_main, LV_PART_MAIN);
    lv_obj_add_style(ui_RollerMin, &style_roller_selected, LV_PART_SELECTED);

    lv_roller_set_selected(ui_RollerHour, current_hour, LV_ANIM_OFF);
    lv_roller_set_selected(ui_RollerMin, current_min, LV_ANIM_OFF);

    lv_obj_t * week_label = lv_label_create(card);
    lv_label_set_text(week_label, "重复周期");
    lv_obj_set_style_text_font(week_label, &ui_font_simhei14, 0);
    lv_obj_set_style_text_color(week_label, lv_color_hex(0x666666), 0);
    lv_obj_set_style_pad_top(week_label, 10, 0);
    lv_obj_set_style_pad_bottom(week_label, 6, 0);

    lv_obj_t * week_cnt = lv_obj_create(card);
    lv_obj_set_scrollbar_mode(week_cnt, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_width(week_cnt, lv_pct(100));
    lv_obj_set_height(week_cnt, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(week_cnt, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(week_cnt, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(week_cnt, 0, 0);
    lv_obj_set_style_border_width(week_cnt, 0, 0);
    lv_obj_set_style_pad_column(week_cnt, 2, LV_PART_MAIN);

    const char * week_names[] = {"一", "二", "三", "四", "五", "六", "日"};
    uint8_t week_masks[] = {
        ALARM_REPEAT_MON, ALARM_REPEAT_TUE, ALARM_REPEAT_WED, 
        ALARM_REPEAT_THU, ALARM_REPEAT_FRI, ALARM_REPEAT_SAT, ALARM_REPEAT_SUN
    };

    for (int i = 0; i < 7; i++) {
        lv_obj_t * w_btn = lv_btn_create(week_cnt);
        lv_obj_set_size(w_btn, 32, 32);
        lv_obj_set_style_radius(w_btn, LV_RADIUS_CIRCLE, 0);
        lv_obj_add_flag(w_btn, LV_OBJ_FLAG_CHECKABLE);
        
        if (g_selected_repeat_days & week_masks[i]) {
            lv_obj_add_state(w_btn, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(w_btn, LV_STATE_CHECKED);
        }
        
        lv_obj_set_style_bg_color(w_btn, lv_color_hex(0xF2F2F7), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(w_btn, lv_color_hex(0x007AFF), LV_STATE_CHECKED);
        lv_obj_set_style_shadow_width(w_btn, 0, 0);

        lv_obj_t * w_lbl = lv_label_create(w_btn);
        lv_label_set_text(w_lbl, week_names[i]);
        lv_obj_set_style_text_font(w_lbl, &ui_font_simhei14, 0);
        lv_obj_set_style_text_color(w_lbl, lv_color_hex(0x000000), LV_STATE_DEFAULT);
        lv_obj_set_style_text_color(w_lbl, lv_color_hex(0xFFFFFF), LV_STATE_CHECKED);
        lv_obj_center(w_lbl);

        lv_obj_add_event_cb(w_btn, ui_event_WeekBtn_click, LV_EVENT_VALUE_CHANGED, (void*)(uintptr_t)week_masks[i]);
    }

    lv_obj_t * btn_cnt = lv_obj_create(card);
    lv_obj_set_width(btn_cnt, lv_pct(100));
    lv_obj_set_height(btn_cnt, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_cnt, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cnt, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_cnt, 0, 0);
    lv_obj_set_style_border_width(btn_cnt, 0, 0);
    lv_obj_set_style_pad_top(btn_cnt, 16, 0);
    lv_obj_set_style_pad_left(btn_cnt, 0, 0);
    lv_obj_set_style_pad_right(btn_cnt, 0, 0);

    lv_obj_t * btn_left = lv_btn_create(btn_cnt);
    lv_obj_set_width(btn_left, lv_pct(46));
    lv_obj_set_height(btn_left, 42);
    lv_obj_set_style_radius(btn_left, 21, 0);
    lv_obj_set_style_shadow_width(btn_left, 0, 0);

    lv_obj_t * lbl_left = lv_label_create(btn_left);
    lv_obj_set_style_text_font(lbl_left, &ui_font_simhei14, 0);
    lv_obj_center(lbl_left);

    if (item != NULL) {
        lv_obj_set_style_bg_color(btn_left, lv_color_hex(0xFF3B30), 0);
        lv_label_set_text(lbl_left, "删除");
        lv_obj_set_style_text_color(lbl_left, lv_color_hex(0xFFFFFF), 0);
        lv_obj_add_event_cb(btn_left, ui_event_AlarmDialog_delete, LV_EVENT_CLICKED, NULL);
    } else {
        lv_obj_set_style_bg_color(btn_left, lv_color_hex(0xE5E5EA), 0);
        lv_label_set_text(lbl_left, "取消");
        lv_obj_set_style_text_color(lbl_left, lv_color_hex(0x333333), 0);
        lv_obj_add_event_cb(btn_left, ui_event_AlarmDialog_cancel, LV_EVENT_CLICKED, NULL);
    }

    lv_obj_t * btn_save = lv_btn_create(btn_cnt);
    lv_obj_set_width(btn_save, lv_pct(46));
    lv_obj_set_height(btn_save, 42);
    lv_obj_set_style_radius(btn_save, 21, 0);
    lv_obj_set_style_bg_color(btn_save, lv_color_hex(0x007AFF), 0);
    lv_obj_set_style_shadow_width(btn_save, 0, 0);
    lv_obj_add_event_cb(btn_save, ui_event_AlarmDialog_save, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "保存");
    lv_obj_set_style_text_font(lbl_save, &ui_font_simhei14, 0);
    lv_obj_set_style_text_color(lbl_save, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_save);
}

///////////////////// UI BUILD HELPERS //////////////////

static lv_obj_t * ui_AlarmCard_create(lv_obj_t * parent, alarm_item_t * alarm)
{
    lv_obj_t * card = lv_obj_create(parent);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_radius(card, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(card, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(card, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(card, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(card, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, ui_event_AlarmCard_edit, LV_EVENT_CLICKED, alarm);

    lv_obj_t * text_cnt = lv_obj_create(card);
    lv_obj_set_width(text_cnt, lv_pct(68));
    lv_obj_set_height(text_cnt, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text_cnt, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(text_cnt, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(text_cnt, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(text_cnt, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(text_cnt, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * lbl_time = lv_label_create(text_cnt);
    lv_label_set_text(lbl_time, alarm->time_str);
    lv_obj_set_style_text_font(lbl_time, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_time, lv_color_hex(0x111111), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(lbl_time, LV_OBJ_FLAG_CLICKABLE);

    char days_buf[128];
    ui_get_repeat_days_str(alarm->repeat_days, days_buf, sizeof(days_buf));

    char sub_buf[256];
    snprintf(sub_buf, sizeof(sub_buf), "%s | %s", days_buf, alarm->tag_str);

    lv_obj_t * lbl_sub = lv_label_create(text_cnt);
    lv_obj_set_width(lbl_sub, lv_pct(100));
    lv_label_set_long_mode(lbl_sub, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(lbl_sub, sub_buf);
    lv_obj_set_style_text_font(lbl_sub, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_sub, lv_color_hex(0x888888), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(lbl_sub, LV_OBJ_FLAG_CLICKABLE);

    static lv_style_t style_switch_main;
    static lv_style_t style_switch_indicator_checked;
    static lv_style_t style_switch_knob;
    static bool inited = false;
    if(!inited) {
        lv_style_init(&style_switch_main);
        lv_style_set_bg_color(&style_switch_main, lv_color_hex(0xc4d8cb));
        lv_style_set_bg_opa(&style_switch_main, 255);
        lv_style_set_border_width(&style_switch_main, 0);

        lv_style_init(&style_switch_indicator_checked);
        lv_style_set_bg_color(&style_switch_indicator_checked, lv_color_hex(0x22c55e));
        lv_style_set_bg_opa(&style_switch_indicator_checked, 255);
        lv_style_set_border_width(&style_switch_indicator_checked, 0);

        lv_style_init(&style_switch_knob);
        lv_style_set_bg_color(&style_switch_knob, lv_color_hex(0xffffff));
        lv_style_set_bg_opa(&style_switch_knob, 255);
        lv_style_set_pad_all(&style_switch_knob, 2);
        lv_style_set_shadow_color(&style_switch_knob, lv_color_hex(0x000000));
        lv_style_set_shadow_opa(&style_switch_knob, 102);
        lv_style_set_shadow_width(&style_switch_knob, 16);
        lv_style_set_shadow_offset_y(&style_switch_knob, 2);
        inited = true;
    }

    lv_obj_t * sw = lv_switch_create(card);
    lv_obj_set_width(sw, 50);
    lv_obj_set_height(sw, 26);

    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_KNOB | LV_STATE_DEFAULT);

    lv_obj_add_style(sw, &style_switch_main, LV_PART_MAIN);
    lv_obj_add_style(sw, &style_switch_indicator_checked, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_style(sw, &style_switch_knob, LV_PART_KNOB);

    if (alarm->is_enabled) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }

    lv_obj_add_event_cb(sw, ui_event_AlarmSwitch_toggle, LV_EVENT_VALUE_CHANGED, alarm);

    return card;
}

static lv_obj_t * ui_WorldClockCard_create(lv_obj_t * parent, world_clock_item_t * clock_item)
{
    lv_obj_t * card = lv_obj_create(parent);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_radius(card, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(card, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(card, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(card, 12, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_set_user_data(card, clock_item);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, ui_event_WorldClockCard_click, LV_EVENT_CLICKED, clock_item);

    lv_obj_t * text_cnt = lv_obj_create(card);
    lv_obj_set_width(text_cnt, lv_pct(50));
    lv_obj_set_height(text_cnt, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(text_cnt, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(text_cnt, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(text_cnt, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(text_cnt, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(text_cnt, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * lbl_city = lv_label_create(text_cnt);
    lv_label_set_text(lbl_city, clock_item->city_name);
    lv_obj_set_style_text_font(lbl_city, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_city, lv_color_hex(0x222222), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(lbl_city, LV_OBJ_FLAG_CLICKABLE);

    char time_buf[16];
    char diff_buf[64];
    ui_calc_world_clock_data(clock_item->tz_offset_hours, time_buf, sizeof(time_buf), diff_buf, sizeof(diff_buf));

    lv_obj_t * lbl_diff = lv_label_create(text_cnt);
    lv_label_set_text(lbl_diff, diff_buf);
    lv_obj_set_style_text_font(lbl_diff, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_diff, lv_color_hex(0x999999), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(lbl_diff, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t * lbl_time = lv_label_create(card);
    lv_label_set_text(lbl_time, time_buf);
    lv_obj_set_style_text_font(lbl_time, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(lbl_time, lv_color_hex(0x111111), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(lbl_time, LV_OBJ_FLAG_CLICKABLE);

    return card;
}

static lv_obj_t * ui_create_page_floating_btn(lv_obj_t * parent, const char * text, lv_color_t bg_color, bool is_round)
{
    lv_obj_t * btn = lv_btn_create(parent);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_FLOATING);

    if (is_round) {
        lv_obj_set_size(btn, 50, 50);
        lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
    } else {
        lv_obj_set_size(btn, 110, 42);
        lv_obj_set_style_radius(btn, 21, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    lv_obj_set_align(btn, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(btn, -15);
    lv_obj_set_style_bg_color(btn, bg_color, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_align(lbl, LV_ALIGN_CENTER);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);

    return btn;
}

static lv_obj_t * ui_TabItem_create(lv_obj_t * parent, const char * icon, const char * text)
{
    lv_obj_t * btn = lv_btn_create(parent);
    lv_obj_set_width(btn, lv_pct(24));
    lv_obj_set_height(btn, lv_pct(100));
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * icon_lbl = lv_label_create(btn);
    lv_label_set_text(icon_lbl, icon);

    lv_obj_t * text_lbl = lv_label_create(btn);
    lv_label_set_text(text_lbl, text);
    lv_obj_set_style_text_font(text_lbl, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);

    return btn;
}

static void ui_set_tab_item_color(lv_obj_t * tab_btn, lv_color_t color)
{
    if (tab_btn == NULL) return;
    lv_obj_t * icon = lv_obj_get_child(tab_btn, 0);
    lv_obj_t * text = lv_obj_get_child(tab_btn, 1);
    if (icon) lv_obj_set_style_text_color(icon, color, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (text) lv_obj_set_style_text_color(text, color, LV_PART_MAIN | LV_STATE_DEFAULT);
}

static void ui_update_tab_bar_colors(lv_obj_t * active_tab)
{
    lv_color_t active_color = lv_color_hex(0x0A84FF);
    lv_color_t inactive_color = lv_color_hex(0x8E8E93);

    ui_set_tab_item_color(ui_TabBtnAlarm, (active_tab == ui_TabBtnAlarm) ? active_color : inactive_color);
    ui_set_tab_item_color(ui_TabBtnWorld, (active_tab == ui_TabBtnWorld) ? active_color : inactive_color);
    ui_set_tab_item_color(ui_TabBtnStopwatch, (active_tab == ui_TabBtnStopwatch) ? active_color : inactive_color);
    ui_set_tab_item_color(ui_TabBtnTimer, (active_tab == ui_TabBtnTimer) ? active_color : inactive_color);
}

///////////////////// STOPWATCH IMPLEMENTATION //////////////////

static void ui_stopwatch_update_display(void)
{
    if (ui_StopwatchTimeLabel == NULL) return;

    uint32_t total_sec = g_stopwatch_ms / 1000;
    uint32_t mins = total_sec / 60;
    uint32_t secs = total_sec % 60;
    uint32_t ms_hundredths = (g_stopwatch_ms % 1000) / 10;

    char time_str[32];
    snprintf(time_str, sizeof(time_str), "%02u:%02u.%02u", (unsigned int) mins, (unsigned int) secs, (unsigned int) ms_hundredths);
    lv_label_set_text(ui_StopwatchTimeLabel, time_str);
}

static void ui_stopwatch_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);
    if (g_stopwatch_state == STOPWATCH_STATE_RUNNING) {
        g_stopwatch_ms += 30;
        ui_stopwatch_update_display();
    }
}

static void ui_stopwatch_add_lap_item(size_t lap_index, uint32_t ms_val)
{
    if (ui_StopwatchLapListCnt == NULL) return;

    uint32_t total_sec = ms_val / 1000;
    uint32_t mins = total_sec / 60;
    uint32_t secs = total_sec % 60;
    uint32_t ms_hundredths = (ms_val % 1000) / 10;

    char lap_str[32];
    snprintf(lap_str, sizeof(lap_str), "计圈 %zu", lap_index + 1);

    char val_str[32];
    snprintf(val_str, sizeof(val_str), "%02u:%02u.%02u", (unsigned int) mins, (unsigned int) secs, (unsigned int) ms_hundredths);

    lv_obj_t * row = lv_obj_create(ui_StopwatchLapListCnt);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, 36);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(row, 8, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_left(row, 12, 0);
    lv_obj_set_style_pad_right(row, 12, 0);

    lv_obj_t * lbl_title = lv_label_create(row);
    lv_label_set_text(lbl_title, lap_str);
    lv_obj_set_style_text_font(lbl_title, &ui_font_simhei14, 0);
    lv_obj_set_style_text_color(lbl_title, lv_color_hex(0x666666), 0);

    lv_obj_t * lbl_val = lv_label_create(row);
    lv_label_set_text(lbl_val, val_str);
    lv_obj_set_style_text_font(lbl_val, &ui_font_simhei14, 0);
    lv_obj_set_style_text_color(lbl_val, lv_color_hex(0x111111), 0);

    // 将最新的计圈显示在最上方
    lv_obj_move_background(row);
}

static void ui_event_StopwatchStartPause_click(lv_event_t * e)
{
    LV_UNUSED(e);

    if (g_stopwatch_state == STOPWATCH_STATE_STOPPED || g_stopwatch_state == STOPWATCH_STATE_PAUSED) {
        g_stopwatch_state = STOPWATCH_STATE_RUNNING;

        if (g_stopwatch_timer == NULL) {
            g_stopwatch_timer = lv_timer_create(ui_stopwatch_timer_cb, 30, NULL);
        }

        if (ui_StopwatchBtnStartPauseLbl) lv_label_set_text(ui_StopwatchBtnStartPauseLbl, "暂停");
        if (ui_StopwatchBtnStartPause) lv_obj_set_style_bg_color(ui_StopwatchBtnStartPause, lv_color_hex(0xFF9500), 0);

        if (ui_StopwatchBtnLapResetLbl) lv_label_set_text(ui_StopwatchBtnLapResetLbl, "计圈");
        if (ui_StopwatchBtnLapReset) lv_obj_clear_state(ui_StopwatchBtnLapReset, LV_STATE_DISABLED);
    } else {
        g_stopwatch_state = STOPWATCH_STATE_PAUSED;

        if (ui_StopwatchBtnStartPauseLbl) lv_label_set_text(ui_StopwatchBtnStartPauseLbl, "继续");
        if (ui_StopwatchBtnStartPause) lv_obj_set_style_bg_color(ui_StopwatchBtnStartPause, lv_color_hex(0x34C759), 0);

        if (ui_StopwatchBtnLapResetLbl) lv_label_set_text(ui_StopwatchBtnLapResetLbl, "复位");
    }
}

static void ui_event_StopwatchLapReset_click(lv_event_t * e)
{
    LV_UNUSED(e);

    if (g_stopwatch_state == STOPWATCH_STATE_RUNNING) {
        if (g_stopwatch_lap_count < MAX_STOPWATCH_LAPS) {
            g_stopwatch_laps[g_stopwatch_lap_count] = g_stopwatch_ms;
            ui_stopwatch_add_lap_item(g_stopwatch_lap_count, g_stopwatch_ms);
            g_stopwatch_lap_count++;
        }
    } else if (g_stopwatch_state == STOPWATCH_STATE_PAUSED) {
        g_stopwatch_state = STOPWATCH_STATE_STOPPED;
        g_stopwatch_ms = 0;
        g_stopwatch_lap_count = 0;

        if (g_stopwatch_timer != NULL) {
            lv_timer_del(g_stopwatch_timer);
            g_stopwatch_timer = NULL;
        }

        ui_stopwatch_update_display();

        if (ui_StopwatchLapListCnt != NULL) {
            lv_obj_clean(ui_StopwatchLapListCnt);
        }

        if (ui_StopwatchBtnStartPauseLbl) lv_label_set_text(ui_StopwatchBtnStartPauseLbl, "开始");
        if (ui_StopwatchBtnStartPause) lv_obj_set_style_bg_color(ui_StopwatchBtnStartPause, lv_color_hex(0x34C759), 0);

        if (ui_StopwatchBtnLapResetLbl) lv_label_set_text(ui_StopwatchBtnLapResetLbl, "计圈");
        if (ui_StopwatchBtnLapReset) lv_obj_add_state(ui_StopwatchBtnLapReset, LV_STATE_DISABLED);
    }
}

static void ui_stopwatch_cleanup(void)
{
    if (g_stopwatch_timer != NULL) {
        lv_timer_del(g_stopwatch_timer);
        g_stopwatch_timer = NULL;
    }
    ui_StopwatchTimeLabel = NULL;
    ui_StopwatchBtnStartPause = NULL;
    ui_StopwatchBtnStartPauseLbl = NULL;
    ui_StopwatchBtnLapReset = NULL;
    ui_StopwatchBtnLapResetLbl = NULL;
    ui_StopwatchLapListCnt = NULL;
}

///////////////////// TIMER IMPLEMENTATION //////////////////

static void ui_timer_update_display(void)
{
    if (ui_TimerTimeLabel == NULL) return;

    uint32_t hrs = g_timer_remaining_sec / 3600;
    uint32_t mins = (g_timer_remaining_sec % 3600) / 60;
    uint32_t secs = g_timer_remaining_sec % 60;

    char time_str[32];
    snprintf(time_str, sizeof(time_str), "%02u:%02u:%02u", (unsigned int) hrs, (unsigned int) mins, (unsigned int) secs);
    lv_label_set_text(ui_TimerTimeLabel, time_str);
}

static void ui_event_TimerAlert_close(lv_event_t * e)
{
    LV_UNUSED(e);
    if (ui_TimerAlertDialog != NULL) {
        lv_obj_del(ui_TimerAlertDialog);
        ui_TimerAlertDialog = NULL;
    }
}

static void ui_timer_show_alert_dialog(void)
{
    if (ui_TimerAlertDialog != NULL) return;

    ui_TimerAlertDialog = lv_obj_create(lv_scr_act());
    lv_obj_add_flag(ui_TimerAlertDialog, LV_OBJ_FLAG_FLOATING);
    lv_obj_set_size(ui_TimerAlertDialog, lv_pct(100), lv_pct(100));
    lv_obj_align(ui_TimerAlertDialog, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(ui_TimerAlertDialog, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ui_TimerAlertDialog, 160, 0);
    lv_obj_set_style_border_width(ui_TimerAlertDialog, 0, 0);
    lv_obj_set_style_pad_all(ui_TimerAlertDialog, 0, 0);

    lv_obj_t * card = lv_obj_create(ui_TimerAlertDialog);
    lv_obj_set_width(card, lv_pct(80));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_center(card);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(card, 20, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * title = lv_label_create(card);
    lv_label_set_text(title, "计时完成！");
    lv_obj_set_style_text_font(title, &ui_font_simhei14, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x111111), 0);
    lv_obj_set_style_pad_bottom(title, 16, 0);

    lv_obj_t * btn_ok = lv_btn_create(card);
    lv_obj_set_width(btn_ok, lv_pct(60));
    lv_obj_set_height(btn_ok, 40);
    lv_obj_set_style_radius(btn_ok, 20, 0);
    lv_obj_set_style_bg_color(btn_ok, lv_color_hex(0x007AFF), 0);
    lv_obj_add_event_cb(btn_ok, ui_event_TimerAlert_close, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_ok = lv_label_create(btn_ok);
    lv_label_set_text(lbl_ok, "知道了");
    lv_obj_set_style_text_font(lbl_ok, &ui_font_simhei14, 0);
    lv_obj_set_style_text_color(lbl_ok, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_ok);
}

static void ui_timer_timer_cb(lv_timer_t * timer)
{
    LV_UNUSED(timer);

    if (g_timer_state == TIMER_STATE_RUNNING) {
        if (g_timer_remaining_sec > 0) {
            g_timer_remaining_sec--;
            ui_timer_update_display();
        }

        if (g_timer_remaining_sec == 0) {
            g_timer_state = TIMER_STATE_SETTING;

            if (g_timer_timer != NULL) {
                lv_timer_del(g_timer_timer);
                g_timer_timer = NULL;
            }

            if (ui_TimerRollerCnt) lv_obj_clear_flag(ui_TimerRollerCnt, LV_OBJ_FLAG_HIDDEN);
            if (ui_TimerDisplayCnt) lv_obj_add_flag(ui_TimerDisplayCnt, LV_OBJ_FLAG_HIDDEN);
            if (ui_TimerBtnCancel) lv_obj_add_flag(ui_TimerBtnCancel, LV_OBJ_FLAG_HIDDEN);

            if (ui_TimerBtnStartPauseLbl) lv_label_set_text(ui_TimerBtnStartPauseLbl, "开始计时");
            if (ui_TimerBtnStartPause) lv_obj_set_style_bg_color(ui_TimerBtnStartPause, lv_color_hex(0xFF9500), 0);

            ui_timer_show_alert_dialog();
        }
    }
}

static void ui_event_TimerStartPause_click(lv_event_t * e)
{
    LV_UNUSED(e);

    if (g_timer_state == TIMER_STATE_SETTING) {
        uint16_t h = lv_roller_get_selected(ui_TimerRollerH);
        uint16_t m = lv_roller_get_selected(ui_TimerRollerM);
        uint16_t s = lv_roller_get_selected(ui_TimerRollerS);

        g_timer_total_sec = h * 3600 + m * 60 + s;
        if (g_timer_total_sec == 0) return;

        g_timer_remaining_sec = g_timer_total_sec;
        g_timer_state = TIMER_STATE_RUNNING;

        if (g_timer_timer == NULL) {
            g_timer_timer = lv_timer_create(ui_timer_timer_cb, 1000, NULL);
        }

        if (ui_TimerRollerCnt) lv_obj_add_flag(ui_TimerRollerCnt, LV_OBJ_FLAG_HIDDEN);
        if (ui_TimerDisplayCnt) lv_obj_clear_flag(ui_TimerDisplayCnt, LV_OBJ_FLAG_HIDDEN);
        if (ui_TimerBtnCancel) lv_obj_clear_flag(ui_TimerBtnCancel, LV_OBJ_FLAG_HIDDEN);

        ui_timer_update_display();

        if (ui_TimerBtnStartPauseLbl) lv_label_set_text(ui_TimerBtnStartPauseLbl, "暂停");
        if (ui_TimerBtnStartPause) lv_obj_set_style_bg_color(ui_TimerBtnStartPause, lv_color_hex(0xFF9500), 0);
    } else if (g_timer_state == TIMER_STATE_RUNNING) {
        g_timer_state = TIMER_STATE_PAUSED;

        if (ui_TimerBtnStartPauseLbl) lv_label_set_text(ui_TimerBtnStartPauseLbl, "继续");
        if (ui_TimerBtnStartPause) lv_obj_set_style_bg_color(ui_TimerBtnStartPause, lv_color_hex(0x34C759), 0);
    } else if (g_timer_state == TIMER_STATE_PAUSED) {
        g_timer_state = TIMER_STATE_RUNNING;

        if (ui_TimerBtnStartPauseLbl) lv_label_set_text(ui_TimerBtnStartPauseLbl, "暂停");
        if (ui_TimerBtnStartPause) lv_obj_set_style_bg_color(ui_TimerBtnStartPause, lv_color_hex(0xFF9500), 0);
    }
}

static void ui_event_TimerCancel_click(lv_event_t * e)
{
    LV_UNUSED(e);

    g_timer_state = TIMER_STATE_SETTING;

    if (g_timer_timer != NULL) {
        lv_timer_del(g_timer_timer);
        g_timer_timer = NULL;
    }

    if (ui_TimerRollerCnt) lv_obj_clear_flag(ui_TimerRollerCnt, LV_OBJ_FLAG_HIDDEN);
    if (ui_TimerDisplayCnt) lv_obj_add_flag(ui_TimerDisplayCnt, LV_OBJ_FLAG_HIDDEN);
    if (ui_TimerBtnCancel) lv_obj_add_flag(ui_TimerBtnCancel, LV_OBJ_FLAG_HIDDEN);

    if (ui_TimerBtnStartPauseLbl) lv_label_set_text(ui_TimerBtnStartPauseLbl, "开始计时");
    if (ui_TimerBtnStartPause) lv_obj_set_style_bg_color(ui_TimerBtnStartPause, lv_color_hex(0xFF9500), 0);
}

static void ui_timer_cleanup(void)
{
    if (g_timer_timer != NULL) {
        lv_timer_del(g_timer_timer);
        g_timer_timer = NULL;
    }
    ui_TimerDisplayCnt = NULL;
    ui_TimerTimeLabel = NULL;
    ui_TimerRollerCnt = NULL;
    ui_TimerRollerH = NULL;
    ui_TimerRollerM = NULL;
    ui_TimerRollerS = NULL;
    ui_TimerBtnStartPause = NULL;
    ui_TimerBtnStartPauseLbl = NULL;
    ui_TimerBtnCancel = NULL;
    ui_TimerAlertDialog = NULL;
}

///////////////////// PAGE BUILDERS //////////////////

// 1. 闹钟列表页面
static void ui_build_alarm_list(lv_obj_t * parent)
{
    g_is_world_clock_active = false;
    ui_stop_world_clock_timer();
    ui_stopwatch_cleanup();
    ui_timer_cleanup();

    ui_TitleLabel = lv_label_create(parent);
    lv_label_set_text(ui_TitleLabel, "闹钟");
    lv_obj_set_style_text_font(ui_TitleLabel, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_TitleLabel, lv_color_hex(0x111111), LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_SubTitleLabel = lv_label_create(parent);
    lv_obj_set_style_text_font(ui_SubTitleLabel, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_SubTitleLabel, lv_color_hex(0x888888), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_SubTitleLabel, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_update_alarm_subtitle();

    for (size_t i = 0; i < g_alarm_count; i++) {
        ui_AlarmCard_create(parent, &g_alarm_list[i]);
    }

    lv_obj_t * add_btn = ui_create_page_floating_btn(parent, "+", lv_color_hex(0x007AFF), true);
    lv_obj_add_event_cb(add_btn, ui_event_AddAlarm_click, LV_EVENT_CLICKED, NULL);
}

// 2. 世界时钟页面
static void ui_build_world_clock(lv_obj_t * parent)
{
    g_is_world_clock_active = true;
    ui_stopwatch_cleanup();
    ui_timer_cleanup();

    ui_TitleLabel = lv_label_create(parent);
    lv_label_set_text(ui_TitleLabel, "世界时钟");
    lv_obj_set_style_text_font(ui_TitleLabel, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_TitleLabel, lv_color_hex(0x111111), LV_PART_MAIN | LV_STATE_DEFAULT);

    for (size_t i = 0; i < g_world_clock_count; i++) {
        ui_WorldClockCard_create(parent, &g_world_clock_list[i]);
    }

    lv_obj_t * add_btn = ui_create_page_floating_btn(parent, "+", lv_color_hex(0x34C759), true);
    lv_obj_add_event_cb(add_btn, ui_event_AddWorldClock_click, LV_EVENT_CLICKED, NULL);

    ui_start_world_clock_timer();
}

// 3. 秒表页面
static void ui_build_stopwatch(lv_obj_t * parent)
{
    g_is_world_clock_active = false;
    ui_stop_world_clock_timer();
    ui_timer_cleanup();

    ui_TitleLabel = lv_label_create(parent);
    lv_label_set_text(ui_TitleLabel, "秒表");
    lv_obj_set_style_text_font(ui_TitleLabel, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_TitleLabel, lv_color_hex(0x111111), LV_PART_MAIN | LV_STATE_DEFAULT);

    // 时间显示卡片
    lv_obj_t * display_cnt = lv_obj_create(parent);
    lv_obj_set_width(display_cnt, lv_pct(100));
    lv_obj_set_height(display_cnt, lv_pct(20));
    lv_obj_set_style_bg_color(display_cnt, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(display_cnt, 16, 0);
    lv_obj_set_style_border_width(display_cnt, 0, 0);

    ui_StopwatchTimeLabel = lv_label_create(display_cnt);
    ui_stopwatch_update_display();
    lv_obj_set_style_text_font(ui_StopwatchTimeLabel, &ui_font_simhei14, 0);
    lv_obj_align(ui_StopwatchTimeLabel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(ui_StopwatchTimeLabel, lv_color_hex(0x111111), 0);

    // 双按钮布局区域（复位/计圈，开始/暂停）
    lv_obj_t * btn_cnt = lv_obj_create(parent);
    lv_obj_set_width(btn_cnt, lv_pct(100));
    lv_obj_set_height(btn_cnt, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_cnt, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cnt, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_cnt, 0, 0);
    lv_obj_set_style_border_width(btn_cnt, 0, 0);
    lv_obj_set_style_pad_all(btn_cnt, 0, 0);

    // 左侧按钮：计圈 / 复位
    ui_StopwatchBtnLapReset = lv_btn_create(btn_cnt);
    lv_obj_set_width(ui_StopwatchBtnLapReset, lv_pct(46));
    lv_obj_set_height(ui_StopwatchBtnLapReset, 42);
    lv_obj_set_style_radius(ui_StopwatchBtnLapReset, 21, 0);
    lv_obj_set_style_bg_color(ui_StopwatchBtnLapReset, lv_color_hex(0x8E8E93), 0);
    lv_obj_set_style_shadow_width(ui_StopwatchBtnLapReset, 0, 0);
    lv_obj_add_event_cb(ui_StopwatchBtnLapReset, ui_event_StopwatchLapReset_click, LV_EVENT_CLICKED, NULL);

    ui_StopwatchBtnLapResetLbl = lv_label_create(ui_StopwatchBtnLapReset);
    lv_label_set_text(ui_StopwatchBtnLapResetLbl, (g_stopwatch_state == STOPWATCH_STATE_PAUSED) ? "复位" : "计圈");
    lv_obj_set_style_text_font(ui_StopwatchBtnLapResetLbl, &ui_font_simhei14, 0);
    lv_obj_set_style_text_color(ui_StopwatchBtnLapResetLbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(ui_StopwatchBtnLapResetLbl);

    if (g_stopwatch_state == STOPWATCH_STATE_STOPPED) {
        lv_obj_add_state(ui_StopwatchBtnLapReset, LV_STATE_DISABLED);
    }

    // 右侧按钮：开始 / 暂停 / 继续
    ui_StopwatchBtnStartPause = lv_btn_create(btn_cnt);
    lv_obj_set_width(ui_StopwatchBtnStartPause, lv_pct(46));
    lv_obj_set_height(ui_StopwatchBtnStartPause, 42);
    lv_obj_set_style_radius(ui_StopwatchBtnStartPause, 21, 0);
    
    lv_color_t btn_color = (g_stopwatch_state == STOPWATCH_STATE_RUNNING) ? lv_color_hex(0xFF9500) : lv_color_hex(0x34C759);
    lv_obj_set_style_bg_color(ui_StopwatchBtnStartPause, btn_color, 0);
    lv_obj_set_style_shadow_width(ui_StopwatchBtnStartPause, 0, 0);
    lv_obj_add_event_cb(ui_StopwatchBtnStartPause, ui_event_StopwatchStartPause_click, LV_EVENT_CLICKED, NULL);

    ui_StopwatchBtnStartPauseLbl = lv_label_create(ui_StopwatchBtnStartPause);
    const char * start_lbl_text = "开始";
    if (g_stopwatch_state == STOPWATCH_STATE_RUNNING) start_lbl_text = "暂停";
    else if (g_stopwatch_state == STOPWATCH_STATE_PAUSED) start_lbl_text = "继续";
    
    lv_label_set_text(ui_StopwatchBtnStartPauseLbl, start_lbl_text);
    lv_obj_set_style_text_font(ui_StopwatchBtnStartPauseLbl, &ui_font_simhei14, 0);
    lv_obj_set_style_text_color(ui_StopwatchBtnStartPauseLbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(ui_StopwatchBtnStartPauseLbl);

    // 计圈列表可滚动容器
    ui_StopwatchLapListCnt = lv_obj_create(parent);
    lv_obj_set_width(ui_StopwatchLapListCnt, lv_pct(100));
    lv_obj_set_flex_grow(ui_StopwatchLapListCnt, 1);
    lv_obj_set_flex_flow(ui_StopwatchLapListCnt, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_bg_opa(ui_StopwatchLapListCnt, 0, 0);
    lv_obj_set_style_border_width(ui_StopwatchLapListCnt, 0, 0);
    lv_obj_set_style_pad_all(ui_StopwatchLapListCnt, 0, 0);
    lv_obj_set_style_pad_row(ui_StopwatchLapListCnt, 6, 0);

    for (size_t i = 0; i < g_stopwatch_lap_count; i++) {
        ui_stopwatch_add_lap_item(i, g_stopwatch_laps[i]);
    }
}

// 4. 计时器页面
static void ui_build_timer(lv_obj_t * parent)
{
    g_is_world_clock_active = false;
    ui_stop_world_clock_timer();
    ui_stopwatch_cleanup();

    ui_TitleLabel = lv_label_create(parent);
    lv_label_set_text(ui_TitleLabel, "计时器");
    lv_obj_set_style_text_font(ui_TitleLabel, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(ui_TitleLabel, lv_color_hex(0x111111), LV_PART_MAIN | LV_STATE_DEFAULT);

    // 倒计时文本大屏
    ui_TimerDisplayCnt = lv_obj_create(parent);
    lv_obj_set_width(ui_TimerDisplayCnt, lv_pct(100));
    lv_obj_set_height(ui_TimerDisplayCnt, lv_pct(50));
    lv_obj_set_style_bg_color(ui_TimerDisplayCnt, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(ui_TimerDisplayCnt, 16, 0);
    lv_obj_set_style_border_width(ui_TimerDisplayCnt, 0, 0);

    ui_TimerTimeLabel = lv_label_create(ui_TimerDisplayCnt);
    ui_timer_update_display();
    lv_obj_set_style_text_font(ui_TimerTimeLabel, &ui_font_simhei14, 0);
    lv_obj_align(ui_TimerTimeLabel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(ui_TimerTimeLabel, lv_color_hex(0x111111), 0);

    // 时分秒三 Wheel 选择器容器
    ui_TimerRollerCnt = lv_obj_create(parent);
    lv_obj_set_width(ui_TimerRollerCnt, lv_pct(100));
    lv_obj_set_height(ui_TimerRollerCnt, lv_pct(50));
    lv_obj_clear_flag(ui_TimerRollerCnt, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(ui_TimerRollerCnt, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_TimerRollerCnt, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(ui_TimerRollerCnt, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(ui_TimerRollerCnt, 16, 0);
    lv_obj_set_style_border_width(ui_TimerRollerCnt, 0, 0);
    

    char hour_options[128] = "";
    for (int i = 0; i < 24; i++) snprintf(hour_options + strlen(hour_options), sizeof(hour_options) - strlen(hour_options), "%02d\n", i);
    hour_options[strlen(hour_options) - 1] = '\0';

    char ms_options[320] = "";
    for (int i = 0; i < 60; i++) snprintf(ms_options + strlen(ms_options), sizeof(ms_options) - strlen(ms_options), "%02d\n", i);
    ms_options[strlen(ms_options) - 1] = '\0';

    static lv_style_t style_roller_main;
    static lv_style_t style_roller_selected;
    static bool inited = false;
    if(!inited) {
        lv_style_init(&style_roller_main);
        lv_style_set_bg_color(&style_roller_main, lv_color_hex(0xf3f4f6));
        lv_style_set_bg_opa(&style_roller_main, 255);
        lv_style_set_radius(&style_roller_main, 12);
        lv_style_set_border_color(&style_roller_main, lv_color_hex(0xd1d5db));
        lv_style_set_border_width(&style_roller_main, 1);
        lv_style_set_text_color(&style_roller_main, lv_color_hex(0x6b7280));

        lv_style_init(&style_roller_selected);
        lv_style_set_bg_color(&style_roller_selected, lv_color_hex(0x6366f1));
        lv_style_set_bg_opa(&style_roller_selected, 255);
        lv_style_set_text_color(&style_roller_selected, lv_color_hex(0xffffff));
        inited = true;
    }

    ui_TimerRollerH = lv_roller_create(ui_TimerRollerCnt);
    lv_roller_set_options(ui_TimerRollerH, hour_options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(ui_TimerRollerH, 5);
    lv_obj_set_width(ui_TimerRollerH, lv_pct(28));
    lv_obj_set_style_text_align(ui_TimerRollerH, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(ui_TimerRollerH, &style_roller_main, LV_PART_MAIN);
    lv_obj_add_style(ui_TimerRollerH, &style_roller_selected, LV_PART_SELECTED);

    ui_TimerRollerM = lv_roller_create(ui_TimerRollerCnt);
    lv_roller_set_options(ui_TimerRollerM, ms_options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(ui_TimerRollerM, 5);
    lv_obj_set_width(ui_TimerRollerM, lv_pct(28));
    lv_obj_set_style_text_align(ui_TimerRollerM, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(ui_TimerRollerM, &style_roller_main, LV_PART_MAIN);
    lv_obj_add_style(ui_TimerRollerM, &style_roller_selected, LV_PART_SELECTED);

    ui_TimerRollerS = lv_roller_create(ui_TimerRollerCnt);
    lv_roller_set_options(ui_TimerRollerS, ms_options, LV_ROLLER_MODE_NORMAL);
    lv_roller_set_visible_row_count(ui_TimerRollerS, 5);
    lv_obj_set_width(ui_TimerRollerS, lv_pct(28));
    lv_obj_set_style_text_align(ui_TimerRollerS, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(ui_TimerRollerS, &style_roller_main, LV_PART_MAIN);
    lv_obj_add_style(ui_TimerRollerS, &style_roller_selected, LV_PART_SELECTED);

    lv_roller_set_selected(ui_TimerRollerH, g_timer_total_sec / 3600, LV_ANIM_OFF);
    lv_roller_set_selected(ui_TimerRollerM, (g_timer_total_sec % 3600) / 60, LV_ANIM_OFF);
    lv_roller_set_selected(ui_TimerRollerS, g_timer_total_sec % 60, LV_ANIM_OFF);

    if (g_timer_state == TIMER_STATE_SETTING) {
        lv_obj_add_flag(ui_TimerDisplayCnt, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(ui_TimerRollerCnt, LV_OBJ_FLAG_HIDDEN);
    }

    // 控制按钮区
    lv_obj_t * btn_cnt = lv_obj_create(parent);
    lv_obj_set_width(btn_cnt, lv_pct(100));
    // lv_obj_set_height(btn_cnt, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(btn_cnt, 1);
    lv_obj_set_flex_flow(btn_cnt, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cnt, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(btn_cnt, 0, 0);
    lv_obj_set_style_border_width(btn_cnt, 0, 0);
    lv_obj_set_style_pad_all(btn_cnt, 0, 0);

    // 取消按钮
    ui_TimerBtnCancel = lv_btn_create(btn_cnt);
    lv_obj_set_width(ui_TimerBtnCancel, lv_pct(46));
    lv_obj_set_height(ui_TimerBtnCancel, 42);
    lv_obj_set_style_radius(ui_TimerBtnCancel, 21, 0);
    lv_obj_set_style_bg_color(ui_TimerBtnCancel, lv_color_hex(0x8E8E93), 0);
    lv_obj_set_style_shadow_width(ui_TimerBtnCancel, 0, 0);
    lv_obj_add_event_cb(ui_TimerBtnCancel, ui_event_TimerCancel_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_cancel = lv_label_create(ui_TimerBtnCancel);
    lv_label_set_text(lbl_cancel, "取消");
    lv_obj_set_style_text_font(lbl_cancel, &ui_font_simhei14, 0);
    lv_obj_set_style_text_color(lbl_cancel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl_cancel);

    if (g_timer_state == TIMER_STATE_SETTING) {
        lv_obj_add_flag(ui_TimerBtnCancel, LV_OBJ_FLAG_HIDDEN);
    }

    // 开始/暂停按钮
    ui_TimerBtnStartPause = lv_btn_create(btn_cnt);
    lv_obj_set_width(ui_TimerBtnStartPause, lv_pct(46));
    lv_obj_set_height(ui_TimerBtnStartPause, 42);
    lv_obj_set_style_radius(ui_TimerBtnStartPause, 21, 0);
    
    lv_color_t t_btn_color = (g_timer_state == TIMER_STATE_RUNNING) ? lv_color_hex(0xFF9500) : lv_color_hex(0x34C759);
    if (g_timer_state == TIMER_STATE_SETTING) t_btn_color = lv_color_hex(0xFF9500);

    lv_obj_set_style_bg_color(ui_TimerBtnStartPause, t_btn_color, 0);
    lv_obj_set_style_shadow_width(ui_TimerBtnStartPause, 0, 0);
    lv_obj_add_event_cb(ui_TimerBtnStartPause, ui_event_TimerStartPause_click, LV_EVENT_CLICKED, NULL);

    ui_TimerBtnStartPauseLbl = lv_label_create(ui_TimerBtnStartPause);
    const char * t_lbl_text = "开始计时";
    if (g_timer_state == TIMER_STATE_RUNNING) t_lbl_text = "暂停";
    else if (g_timer_state == TIMER_STATE_PAUSED) t_lbl_text = "继续";

    lv_label_set_text(ui_TimerBtnStartPauseLbl, t_lbl_text);
    lv_obj_set_style_text_font(ui_TimerBtnStartPauseLbl, &ui_font_simhei14, 0);
    lv_obj_set_style_text_color(ui_TimerBtnStartPauseLbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(ui_TimerBtnStartPauseLbl);
}

///////////////////// TAB EVENTS //////////////////
void ui_event_TabBtnAlarm(lv_event_t * e)
{
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_obj_clean(ui_ContentArea);
        ui_build_alarm_list(ui_ContentArea);
        ui_update_tab_bar_colors(ui_TabBtnAlarm);
    }
}

void ui_event_TabBtnWorld(lv_event_t * e)
{
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_obj_clean(ui_ContentArea);
        ui_build_world_clock(ui_ContentArea);
        ui_update_tab_bar_colors(ui_TabBtnWorld);
    }
}

void ui_event_TabBtnStopwatch(lv_event_t * e)
{
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_obj_clean(ui_ContentArea);
        ui_build_stopwatch(ui_ContentArea);
        ui_update_tab_bar_colors(ui_TabBtnStopwatch);
    }
}

void ui_event_TabBtnTimer(lv_event_t * e)
{
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_obj_clean(ui_ContentArea);
        ui_build_timer(ui_ContentArea);
        ui_update_tab_bar_colors(ui_TabBtnTimer);
    }
}

///////////////////// SCREEN INIT & DESTROY //////////////////
void ui_clock_screen_init(void)
{
    if(ui_clock != NULL) return;

    ui_clock = lv_obj_create(NULL);
    
    lv_obj_add_event_cb(ui_clock, ui_clock_screen_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_clear_flag(ui_clock, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(ui_clock, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(ui_clock, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_clock, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(ui_clock, lv_color_hex(0xF4F4F6), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_clock, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_TopBarPlaceholder = lv_obj_create(ui_clock);
    lv_obj_set_width(ui_TopBarPlaceholder, lv_pct(100));
    lv_obj_set_height(ui_TopBarPlaceholder, 25);
    lv_obj_set_style_bg_color(ui_TopBarPlaceholder, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_TopBarPlaceholder, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_TopBarPlaceholder, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_TopBarPlaceholder, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_clear_flag(ui_TopBarPlaceholder, LV_OBJ_FLAG_SCROLLABLE);

    ui_ContentArea = lv_obj_create(ui_clock);
    lv_obj_set_width(ui_ContentArea, lv_pct(100));
    lv_obj_set_height(ui_ContentArea, 0); 
    lv_obj_set_flex_grow(ui_ContentArea, 1);
    lv_obj_set_flex_flow(ui_ContentArea, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(ui_ContentArea, LV_DIR_VER);
    
    lv_obj_set_style_bg_opa(ui_ContentArea, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_ContentArea, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(ui_ContentArea, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(ui_ContentArea, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(ui_ContentArea, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(ui_ContentArea, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(ui_ContentArea, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_TabBar = lv_obj_create(ui_clock);
    lv_obj_set_width(ui_TabBar, lv_pct(100));
    lv_obj_set_height(ui_TabBar, 60);
    lv_obj_set_flex_flow(ui_TabBar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ui_TabBar, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_set_style_bg_color(ui_TabBar, lv_color_hex(0x1C1C1E), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_TabBar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(ui_TabBar, lv_color_hex(0x38383A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_TabBar, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_side(ui_TabBar, LV_BORDER_SIDE_TOP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(ui_TabBar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_TabBar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_TabBtnAlarm = ui_TabItem_create(ui_TabBar, LV_SYMBOL_BELL, "闹钟");
    lv_obj_add_event_cb(ui_TabBtnAlarm, ui_event_TabBtnAlarm, LV_EVENT_CLICKED, NULL);

    ui_TabBtnWorld = ui_TabItem_create(ui_TabBar, LV_SYMBOL_GPS, "世界时钟");
    lv_obj_add_event_cb(ui_TabBtnWorld, ui_event_TabBtnWorld, LV_EVENT_CLICKED, NULL);

    ui_TabBtnStopwatch = ui_TabItem_create(ui_TabBar, LV_SYMBOL_REFRESH, "秒表");
    lv_obj_add_event_cb(ui_TabBtnStopwatch, ui_event_TabBtnStopwatch, LV_EVENT_CLICKED, NULL);

    ui_TabBtnTimer = ui_TabItem_create(ui_TabBar, LV_SYMBOL_PLAY, "计时器");
    lv_obj_add_event_cb(ui_TabBtnTimer, ui_event_TabBtnTimer, LV_EVENT_CLICKED, NULL);

    ui_update_tab_bar_colors(ui_TabBtnAlarm);
    ui_build_alarm_list(ui_ContentArea);
}

void ui_clock_screen_destroy(void)
{
    g_is_world_clock_active = false;
    ui_stop_world_clock_timer();
    ui_stopwatch_cleanup();
    ui_timer_cleanup();

    if(ui_clock != NULL) {
        lv_obj_del(ui_clock);

        ui_clock = NULL;
        ui_TopBarPlaceholder = NULL;
        ui_ContentArea = NULL;
        ui_TitleLabel = NULL;
        ui_SubTitleLabel = NULL;
        ui_TabBar = NULL;
        ui_TabBtnAlarm = NULL;
        ui_TabBtnWorld = NULL;
        ui_TabBtnStopwatch = NULL;
        ui_TabBtnTimer = NULL;
        ui_AlarmDialog = NULL;
        ui_WorldDialog = NULL;
    }
}