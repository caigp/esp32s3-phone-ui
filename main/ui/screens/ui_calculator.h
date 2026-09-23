// ui_calculator.h
#ifndef UI_CALCULATOR_H
#define UI_CALCULATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// 屏幕与控件变量定义
extern lv_obj_t * ui_calc_screen;
extern lv_obj_t * ui_calc_display_container;
extern lv_obj_t * ui_calc_display_label;
extern lv_obj_t * ui_calc_btnm;

// 函数声明
void ui_calculator_screen_init(void);
void ui_calculator_screen_destroy(void);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif /* UI_CALCULATOR_H */