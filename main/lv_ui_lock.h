#pragma once

#include <sys/lock.h>

extern _lock_t lvgl_api_lock;
extern void ui_lock();
extern void ui_unlock();