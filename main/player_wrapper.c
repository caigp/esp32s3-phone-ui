#include "player_wrapper.h"

static const char *TAG = "player_wrapper";

static void audio_player_delete(esp_player_handle_t player)
{
    if (player == NULL) {
        return;
    }

    esp_player_set_event_cb(player, NULL, NULL);
    esp_player_deinit(player);
}

esp_player_err_t player_init(esp_player_handle_t *player)
{

    uint8_t stream_id = 0;
    esp_audio_render_stream_handle_t stream = NULL;
    if (esp_audio_render_stream_get(s_render, ESP_AUDIO_RENDER_STREAM_ID(stream_id),
                                    &stream) != ESP_AUDIO_RENDER_ERR_OK) {
        ESP_LOGE(TAG, "Failed to get render stream %u", stream_id);
        return ESP_PLAYER_ERR_FAIL;
    }

    esp_player_config_t config = ESP_PLAYER_CONFIG_DEFAULT();
    config.audio_render_hd = stream;   /* esp_audio_render_stream_handle_t，由 esp_audio_render_stream_get() 获取 */
    config.video_render_hd = NULL;   /* esp_video_render_handle_t，由 esp_video_render_create() 创建；纯音频可为 NULL */

    if(esp_player_init(&config, player) != ESP_PLAYER_ERR_OK)
    {
        return ESP_PLAYER_ERR_FAIL;
    }
    ESP_LOGI(TAG, "player init ok");
    return ESP_PLAYER_ERR_OK;

}

void player_deinit(esp_player_handle_t player)
{
    audio_player_delete(player);
}