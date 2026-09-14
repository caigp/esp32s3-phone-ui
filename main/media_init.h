#pragma once

#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "media_lib_adapter.h"
#include "esp_extractor_defaults.h"
#include "esp_audio_dec_default.h"
#include "periph_manager.h"
#include "globals.h"
#include "esp_gmf_pool.h"
#include "esp_gmf_ch_cvt.h"
#include "esp_gmf_bit_cvt.h"
#include "esp_gmf_rate_cvt.h"
#include "esp_gmf_alc.h"
#include "esp_codec_dev.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_audio_render.h"
#include "esp_audio_render_types.h"
#include "lv_ui_lock.h"
#include "lvgl.h"

extern esp_audio_render_handle_t s_render;

extern void global_media_init();
extern void global_media_deinit();

extern void global_media_update_volume(bool handle_sync);

extern lv_obj_t * volume_slider;
extern lv_timer_t * hide_timer;