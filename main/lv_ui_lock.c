#include "lv_ui_lock.h"

_lock_t lvgl_api_lock;

void ui_lock()
{
    _lock_acquire(&lvgl_api_lock);
}

void ui_unlock()
{
    _lock_release(&lvgl_api_lock);
}