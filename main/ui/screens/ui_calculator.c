// ui_calculator.c
#include "ui_calculator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>

// 全局/静态 UI 对象定义
lv_obj_t * ui_calc_screen = NULL;
lv_obj_t * ui_calc_display_container = NULL;
lv_obj_t * ui_calc_display_label = NULL;
lv_obj_t * ui_calc_btnm = NULL;

// 表达式缓冲区与状态
#define CALC_EXPR_LEN 128
static char calc_expr[CALC_EXPR_LEN] = "0";
static bool calc_is_result = false; // 标记当前显示是否为刚计算出的结果

// 按钮矩阵布局定义：
// 行1: AC,  (,    ),    /
// 行2: 7,   8,    9,    *
// 行3: 4,   5,    6,    -
// 行4: 1,   2,    3,    +
// 行5: 0,   .,    DEL,  =
static const char * calc_btnm_map[] = {
    "AC", "(", ")", "/", "\n",
    "7",  "8", "9", "*", "\n",
    "4",  "5", "6", "-", "\n",
    "1",  "2", "3", "+", "\n",
    "0",  ".", "DEL", "=", ""
};

// 控制标志掩码数组：让 '=' 按键启用 LV_BUTTONMATRIX_CTRL_CUSTOM_1 标识
static const lv_buttonmatrix_ctrl_t calc_btnm_ctrl_map[] = {
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0
};

/* ==================== 计算器核心逻辑算法 ==================== */

static int get_precedence(char op) {
    if (op == '+' || op == '-') return 1;
    if (op == '*' || op == '/') return 2;
    return 0;
}

static bool is_operator(char c) {
    return (c == '+' || c == '-' || c == '*' || c == '/');
}

static double apply_op(double a, double b, char op, bool *has_error) {
    switch (op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/':
            if (b == 0.0) {
                *has_error = true;
                return 0;
            }
            return a / b;
        default:
            *has_error = true;
            return 0;
    }
}

static bool evaluate_expression(const char *expr, double *out_val) {
    double values[64];
    int val_top = -1;

    char ops[64];
    int op_top = -1;

    bool error = false;
    int len = strlen(expr);
    int i = 0;

    if (len > 0 && is_operator(expr[len - 1])) {
        return false;
    }

    while (i < len) {
        if (expr[i] == ' ') {
            i++;
            continue;
        }

        if (isdigit((unsigned char)expr[i]) || expr[i] == '.') {
            char *endptr;
            double val = strtod(&expr[i], &endptr);
            if (endptr == &expr[i]) return false;
            if (val_top >= 63) return false;
            values[++val_top] = val;
            i += (endptr - &expr[i]);
        }
        else if (expr[i] == '(') {
            if (op_top >= 63) return false;
            ops[++op_top] = expr[i];
            i++;
        }
        else if (expr[i] == ')') {
            while (op_top >= 0 && ops[op_top] != '(') {
                if (val_top < 1) return false;
                double val2 = values[val_top--];
                double val1 = values[val_top--];
                char op = ops[op_top--];
                double res = apply_op(val1, val2, op, &error);
                if (error) return false;
                values[++val_top] = res;
            }
            if (op_top < 0) return false;
            op_top--;
            i++;
        }
        else if (is_operator(expr[i])) {
            if (expr[i] == '-' && (i == 0 || expr[i - 1] == '(')) {
                if (val_top >= 63) return false;
                values[++val_top] = 0.0;
            }

            while (op_top >= 0 && get_precedence(ops[op_top]) >= get_precedence(expr[i])) {
                if (val_top < 1) return false;
                double val2 = values[val_top--];
                double val1 = values[val_top--];
                char op = ops[op_top--];
                double res = apply_op(val1, val2, op, &error);
                if (error) return false;
                values[++val_top] = res;
            }
            if (op_top >= 63) return false;
            ops[++op_top] = expr[i];
            i++;
        } else {
            return false;
        }
    }

    while (op_top >= 0) {
        if (ops[op_top] == '(') return false;
        if (val_top < 1) return false;
        double val2 = values[val_top--];
        double val1 = values[val_top--];
        char op = ops[op_top--];
        double res = apply_op(val1, val2, op, &error);
        if (error) return false;
        values[++val_top] = res;
    }

    if (val_top != 0) return false;

    *out_val = values[val_top];
    return true;
}

static void calc_btnm_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btnm = lv_event_get_target(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        uint32_t id = lv_buttonmatrix_get_selected_button(btnm);
        const char * txt = lv_buttonmatrix_get_button_text(btnm, id);

        if (txt == NULL) return;

        // 1. 全清按键 (AC)
        if (strcmp(txt, "AC") == 0) {
            strcpy(calc_expr, "0");
            calc_is_result = false;
        }
        // 2. 退格删除按键 (DEL)
        else if (strcmp(txt, "DEL") == 0) {
            if (strcmp(calc_expr, "Error") == 0 || calc_is_result) {
                strcpy(calc_expr, "0");
                calc_is_result = false;
            } else {
                size_t len = strlen(calc_expr);
                if (len > 1) {
                    calc_expr[len - 1] = '\0';
                } else {
                    strcpy(calc_expr, "0");
                }
            }
        }
        // 3. 等于计算按键 (=)
        else if (strcmp(txt, "=") == 0) {
            double result = 0.0;
            if (evaluate_expression(calc_expr, &result)) {
                if (result == (long long)result) {
                    snprintf(calc_expr, sizeof(calc_expr), "%lld", (long long)result);
                } else {
                    snprintf(calc_expr, sizeof(calc_expr), "%.6g", result);
                }
                calc_is_result = true;
            } else {
                strcpy(calc_expr, "Error");
                calc_is_result = true;
            }
        }
        // 4. 输入数字/运算符/括号
        else {
            if (strcmp(calc_expr, "Error") == 0) {
                calc_expr[0] = '\0';
                strncat(calc_expr, txt, sizeof(calc_expr) - strlen(calc_expr) - 1);
                calc_is_result = false;
            } else if (calc_is_result) {
                if (is_operator(txt[0])) {
                    calc_is_result = false;
                    strncat(calc_expr, txt, sizeof(calc_expr) - strlen(calc_expr) - 1);
                } else {
                    calc_expr[0] = '\0';
                    strncat(calc_expr, txt, sizeof(calc_expr) - strlen(calc_expr) - 1);
                    calc_is_result = false;
                }
            } else if (strcmp(calc_expr, "0") == 0) {
                if (strcmp(txt, ".") == 0 || is_operator(txt[0])) {
                    strncat(calc_expr, txt, sizeof(calc_expr) - strlen(calc_expr) - 1);
                } else {
                    calc_expr[0] = '\0';
                    strncat(calc_expr, txt, sizeof(calc_expr) - strlen(calc_expr) - 1);
                }
            } else {
                if (strlen(calc_expr) + strlen(txt) < CALC_EXPR_LEN - 1) {
                    strcat(calc_expr, txt);
                }
            }
        }

        // 更新显示屏
        if (ui_calc_display_label) {
            lv_label_set_text(ui_calc_display_label, calc_expr);
        }
    }
}

/* ==================== 界面初始化与销毁 ==================== */

void ui_calculator_screen_init(void) {
    // 1. 创建屏幕
    ui_calc_screen = lv_obj_create(NULL);
    lv_obj_set_flex_flow(ui_calc_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ui_calc_screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(ui_calc_screen, LV_OBJ_FLAG_SCROLLABLE);

    // 2. 显示容器（全百分比定位）
    ui_calc_display_container = lv_obj_create(ui_calc_screen);
    lv_obj_remove_style_all(ui_calc_display_container);
    lv_obj_set_width(ui_calc_display_container, lv_pct(90));   // 宽度 90%
    lv_obj_set_height(ui_calc_display_container, lv_pct(20));  // 高度 20%
    lv_obj_set_align(ui_calc_display_container, LV_ALIGN_TOP_MID);
    lv_obj_set_style_pad_top(ui_calc_display_container, 10, 0);
    lv_obj_remove_flag(ui_calc_display_container, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);

    // 3. 显示文本标签（右对齐）
    ui_calc_display_label = lv_label_create(ui_calc_display_container);
    lv_obj_set_width(ui_calc_display_label, lv_pct(100));
    lv_obj_set_height(ui_calc_display_label, LV_SIZE_CONTENT);
    lv_obj_set_align(ui_calc_display_label, LV_ALIGN_BOTTOM_RIGHT);
    lv_label_set_text(ui_calc_display_label, "0");
    lv_obj_set_style_text_align(ui_calc_display_label, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);

    // 4. 按键矩阵 Button Matrix
    ui_calc_btnm = lv_buttonmatrix_create(ui_calc_screen);
    lv_buttonmatrix_set_map(ui_calc_btnm, calc_btnm_map);
    lv_buttonmatrix_set_ctrl_map(ui_calc_btnm, calc_btnm_ctrl_map);

    // 基础样式设置（默认状态下：灰色背景）
    static lv_style_t style_bm_item;
    // 按下样式设置（按下状态下：淡蓝色背景）
    static lv_style_t style_bm_pressed;

    static bool inited = false;
    if(!inited) {
        // --- 1. 普通按钮默认样式 ---
        lv_style_init(&style_bm_item);
        lv_style_set_bg_opa(&style_bm_item, LV_OPA_COVER);
        lv_style_set_bg_color(&style_bm_item, lv_color_hex(0xf3f4f6)); // 默认浅灰色
        lv_style_set_radius(&style_bm_item, 8);
        lv_style_set_border_color(&style_bm_item, lv_color_hex(0xd1d5db));
        lv_style_set_border_width(&style_bm_item, 1);
        lv_style_set_text_color(&style_bm_item, lv_color_hex(0x111827));

        // --- 2. 普通按钮按下效果（淡蓝色） ---
        lv_style_init(&style_bm_pressed);
        lv_style_set_bg_opa(&style_bm_pressed, LV_OPA_COVER);
        lv_style_set_bg_color(&style_bm_pressed, lv_color_hex(0x87CEFA)); // 按下显示淡蓝色 (Light Sky Blue)
        lv_style_set_text_color(&style_bm_pressed, lv_color_hex(0xffffff)); // 按下时文字变为白色

        inited = true;
    }

    // 给所有按键项应用默认样式与【按下状态 (LV_STATE_PRESSED)】样式
    lv_obj_add_style(ui_calc_btnm, &style_bm_item, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_add_style(ui_calc_btnm, &style_bm_pressed, LV_PART_ITEMS | LV_STATE_PRESSED);
    
    lv_obj_set_width(ui_calc_btnm, lv_pct(100));
    lv_obj_set_height(ui_calc_btnm, lv_pct(72));
    lv_obj_set_align(ui_calc_btnm, LV_ALIGN_TOP_MID);

    // 添加按钮之间的间距与内边距
    lv_obj_set_style_pad_row(ui_calc_btnm, 4, LV_PART_MAIN);     // 行间距
    lv_obj_set_style_pad_column(ui_calc_btnm, 4, LV_PART_MAIN);  // 列间距
    lv_obj_set_style_pad_all(ui_calc_btnm, 4, LV_PART_MAIN);     // 矩阵四周内边距
    lv_obj_set_style_pad_top(ui_calc_btnm, 10, LV_PART_MAIN);
    lv_obj_set_style_radius(ui_calc_btnm, 8, LV_PART_ITEMS);     // 按钮圆角

    // 注册逻辑点击事件
    lv_obj_add_event_cb(ui_calc_btnm, calc_btnm_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
}

void ui_calculator_screen_destroy(void) {
    if (ui_calc_screen) {
        lv_obj_del(ui_calc_screen);
    }
    ui_calc_screen = NULL;
    ui_calc_display_container = NULL;
    ui_calc_display_label = NULL;
    ui_calc_btnm = NULL;
}