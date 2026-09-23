#ifndef UI_CLOCK_H
#define UI_CLOCK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

// 引入可能用到的字体声明（根据实际项目字体名微调）
LV_FONT_DECLARE(ui_font_simhei14);

///////////////////// ALARM REPEAT MASKS //////////////////
#define ALARM_REPEAT_SUN (1 << 0)
#define ALARM_REPEAT_MON (1 << 1)
#define ALARM_REPEAT_TUE (1 << 2)
#define ALARM_REPEAT_WED (1 << 3)
#define ALARM_REPEAT_THU (1 << 4)
#define ALARM_REPEAT_FRI (1 << 5)
#define ALARM_REPEAT_SAT (1 << 6)
#define ALARM_REPEAT_EVERYDAY (0x7F) // 0b01111111 (周日到周六)

///////////////////// DATA STRUCTURES //////////////////
typedef struct {
    int id;                 // 闹钟唯一标识 ID
    uint8_t hour;           // 小时 (0 - 23)
    uint8_t minute;         // 分钟 (0 - 59)
    const char * time_str;  // 显示的时间文本 (例如: "06:46")
    uint8_t repeat_days;    // 响铃掩码 (如 ALARM_REPEAT_MON | ALARM_REPEAT_TUE，0 代表响一次)
    const char * tag_str;   // 闹钟标签/备注 (例如: "起床")
    bool is_enabled;        // 是否开启开关
} alarm_item_t;

///////////////////// UI OBJECT HANDLES //////////////////
extern lv_obj_t * ui_clock;
extern lv_obj_t * ui_TopBarPlaceholder;
extern lv_obj_t * ui_ContentArea;
extern lv_obj_t * ui_TitleLabel;
extern lv_obj_t * ui_SubTitleLabel;
extern lv_obj_t * ui_TabBar;
extern lv_obj_t * ui_TabBtnAlarm;
extern lv_obj_t * ui_TabBtnWorld;
extern lv_obj_t * ui_TabBtnStopwatch;
extern lv_obj_t * ui_TabBtnTimer;

///////////////////// PUBLIC API FUNCTIONS //////////////////

/**
 * @brief 初始化闹钟主屏幕
 */
void ui_clock_screen_init(void);

/**
 * @brief 销毁闹钟主屏幕及释放内存
 */
void ui_clock_screen_destroy(void);

/**
 * @brief 刷新闹钟副标题（显示距离下次响铃的时间或关闭提示）
 */
void ui_update_alarm_subtitle(void);

/**
 * @brief 获取距离下一次闹钟响铃的时间描述字符串
 * @return 格式化后的提示文字（如 "15分钟后响铃"）
 */
const char * ui_get_next_alarm_remaining_str(void);

///////////////////// EVENT CALLBACK DECLARATIONS //////////////////
void ui_event_TabBtnAlarm(lv_event_t * e);
void ui_event_TabBtnWorld(lv_event_t * e);
void ui_event_TabBtnStopwatch(lv_event_t * e);
void ui_event_TabBtnTimer(lv_event_t * e);

void ui_event_AlarmSwitch_toggle(lv_event_t * e);
void ui_event_AlarmCard_edit(lv_event_t * e);
void ui_event_AddAlarm_click(lv_event_t * e);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* UI_CLOCK_H */