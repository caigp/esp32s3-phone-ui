#pragma once

#include "esp_player.h"
#include "esp_player_types.h"

#include "esp_audio_render.h"
#include "esp_audio_render_types.h"
#include "media_init.h"

extern esp_player_err_t player_init(esp_player_handle_t *player);
extern void player_deinit(esp_player_handle_t player);