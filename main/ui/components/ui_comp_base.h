#ifndef _UI_COMP_BASE_H
#define _UI_COMP_BASE_H

#include "../ui.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UI_COMP_BASE 0
#define UI_COMP_SYSTEM_BAR_CONTAINER 1
#define UI_COMP_DISPLAY_CONTAINER 2

#define _UI_COMP_BASE_NUM 3
lv_obj_t * ui_base_create(lv_obj_t * comp_parent);

#ifdef __cplusplus
} /*extern "C"*/
#endif

#endif
