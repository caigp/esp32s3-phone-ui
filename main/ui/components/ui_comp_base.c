#include "../ui.h"

lv_obj_t * ui_base_create(lv_obj_t * comp_parent)
{

    lv_obj_t *base = lv_obj_create(comp_parent);
    lv_obj_remove_style_all(base);
    lv_obj_remove_flag(base, LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_flex_flow(base, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(base, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    ui_object_set_themeable_style_property(base, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_BG_COLOR,
                                           _ui_theme_color_white);
    ui_object_set_themeable_style_property(base, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_BG_OPA,
                                           _ui_theme_alpha_white);

    lv_obj_t *system_bar_container = lv_obj_create(base);
    lv_obj_remove_style_all(system_bar_container);
    lv_obj_set_height(system_bar_container, 25);
    lv_obj_set_width(system_bar_container, lv_pct(100));
    lv_obj_set_align(system_bar_container, LV_ALIGN_CENTER);
    lv_obj_remove_flag(system_bar_container, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
    lv_obj_set_style_bg_color(system_bar_container, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(system_bar_container, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *display_container = lv_obj_create(base);
    lv_obj_remove_style_all(display_container);
    lv_obj_set_width(display_container, lv_pct(100));
    lv_obj_set_flex_grow(display_container, 1);
    lv_obj_set_align(display_container, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(display_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(display_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_remove_flag(display_container, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags


    lv_obj_t ** children = lv_malloc(sizeof(lv_obj_t *) * _UI_COMP_BASE_NUM);
    children[UI_COMP_BASE] = base;
    children[UI_COMP_SYSTEM_BAR_CONTAINER] = system_bar_container;
    children[UI_COMP_DISPLAY_CONTAINER] = display_container;
    lv_obj_add_event_cb(base, get_component_child_event_cb, LV_EVENT_GET_COMP_CHILD, children);
    lv_obj_add_event_cb(base, del_component_child_event_cb, LV_EVENT_DELETE, children);

    return base;
}

