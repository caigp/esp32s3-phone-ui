
#include "../../ui.h"
#include "ui_music.h"
#include "media_init.h"
#include "player_wrapper.h"
#include "lv_ui_lock.h"
#include "utils/file_utils.h"
#include "time_format.h"

static lv_timer_t *music_lv_timer;
static esp_player_handle_t player = NULL;
static file_list_handle_t handle;
static TaskHandle_t music_task = NULL;
static int music_index = 0;

static const char *TAG = "Music Player";

void switch_ui_pause(void *data)
{
    ESP_LOGI(TAG, "%s", __func__);
    lv_obj_add_flag(ui_play, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(ui_pause, LV_OBJ_FLAG_HIDDEN);
}

void switch_ui_play(void *data)
{
    ESP_LOGI(TAG, "%s", __func__);
    lv_obj_remove_flag(ui_play, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pause, LV_OBJ_FLAG_HIDDEN);
}

static void duration_xcb(void *data) {
    uint64_t *value_ptr = (uint64_t *)data;
    uint64_t duration = *value_ptr;
    lv_label_set_text(ui_music_duration, FORMAT_SECONDS(duration / 1000));
    lv_slider_set_max_value(ui_music_seek, duration / 1000);
}

static void previous_play_music(void *data)
{
    music_index--;
    if (music_index < 0)
    {
        music_index = file_list_get_count(handle) - 1;
        if (music_index < 0)
        {
            music_index = 0;
        }
    }
    file_info_t info;
    if (file_list_get_by_index(handle, music_index, &info) == 0)
    {
        esp_player_stop(player);
        esp_player_data_src_t src = ESP_PLAYER_DATA_SRC(info.path, ESP_PLAYER_MASK_AUDIO);
        esp_player_set_data_src(player, &src);
        esp_player_run(player);

        //设置正在播放的歌名
        lv_label_set_text(ui_music_info_label, info.name);
        lrc_parse_by_audio_path(info.path);
    }
}

static void next_play_music(void *data)
{
    music_index++;
    if (music_index >= file_list_get_count(handle))
    {
        music_index = 0;
    }
    file_info_t info;
    if (file_list_get_by_index(handle, music_index, &info) == 0)
    {
        ESP_LOGI(TAG, "path = %s, name = %s", info.path, info.name);
        esp_player_stop(player);
        esp_player_data_src_t src = ESP_PLAYER_DATA_SRC(info.path, ESP_PLAYER_MASK_AUDIO);
        esp_player_set_data_src(player, &src);
        esp_player_run(player);

        //设置正在播放的歌名
        lv_label_set_text(ui_music_info_label, info.name);
        lrc_parse_by_audio_path(info.path);
    }
}

static void ui_event_item_cb(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    file_info_t * file_info = (file_info_t *)lv_event_get_user_data(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if(event_code == LV_EVENT_CLICKED) {
        ESP_LOGI(TAG, "path = %s size = %d", file_info->path, file_info->size);
        esp_player_stop(player);
        esp_player_data_src_t src = ESP_PLAYER_DATA_SRC(file_info->path, ESP_PLAYER_MASK_AUDIO);
        esp_player_set_data_src(player, &src);
        esp_player_run(player);

        //设置正在播放的歌名
        lv_label_set_text(ui_music_info_label, file_info->name);
        lrc_parse_by_audio_path(file_info->path);

        music_index = (int) lv_obj_get_user_data(obj);
    }
    else if (event_code == LV_EVENT_DELETE)
    {
        if (file_info)
        {
            lv_free(file_info);
        }
    }
}

static void refresh_play_info_cb(lv_timer_t * timer)
{
    uint64_t current_time;
    esp_player_get_play_time(player, &current_time);

    lv_label_set_text(ui_music_cur_time, FORMAT_SECONDS(current_time / 1000));
    lv_slider_set_value(ui_music_seek, current_time / 1000, LV_ANIM_OFF);

    lrc_update_time(current_time);
}

static esp_player_err_t player_event_cb(esp_player_event_msg_t *msg, void *ctx)
{
    ESP_LOGI(TAG, "player_event %d", msg->event_type);
    switch (msg->event_type) {
        case ESP_PLAYER_EVENT_PAUSED:
            lv_async_call(switch_ui_play, NULL);
            if (music_lv_timer)
            {
                lv_timer_pause(music_lv_timer);
            }
            break;
        case ESP_PLAYER_EVENT_PLAYED:
            if (music_lv_timer)
            {
                lv_timer_resume(music_lv_timer);
            }
            lv_async_call(switch_ui_pause, NULL);
            break;
        case ESP_PLAYER_EVENT_FINISHED:
            lv_async_call(next_play_music, NULL);
            break;
        case ESP_PLAYER_EVENT_ERROR:

            break;
        case ESP_PLAYER_EVENT_AUDIO_INFO_PARSED:
            static uint64_t duration = 0;
            esp_player_get_duration(player, &duration);
            ESP_LOGI(TAG, "esp_player_get_duration ok");
            lv_async_call(duration_xcb, &duration);
            break;
        case ESP_PLAYER_EVENT_SEEK_DONE:
            if (music_lv_timer)
            {
                lv_timer_resume(music_lv_timer);
            }
            break;
        default:
            break;
    }
    return ESP_PLAYER_ERR_OK;
}

static void vMusicScannTask(void *arg)
{
    ui_lock();
    lv_obj_clean(ui_Container45);
    ui_unlock();

    ext_filter_t filters[] = {
        {"mp3", 1},
        {"wav", 1},
    };

    char buf[20];
    snprintf(buf, sizeof(buf), "%s/Music", SD_MOUNT_PATH);

    if (handle != NULL)
    {
        file_list_rescan_with_filters(handle, 0, filters, 2, 0);
    }
    else
    {
        handle = file_list_create_ex(buf, 0, filters, 2, 0);
    }
    
    file_list_print_all(handle);

    int32_t count = file_list_get_count(handle);
    ESP_LOGI(TAG, "music count %d", count);
    for (int32_t i = 0; i < count; i++)
    {
        file_info_t file_info;
        if (file_list_get_by_index(handle, i, &file_info) == 0)
        {
            ui_lock();

            lv_obj_t *item = ui_simpleitem_create(ui_Container45);
            lv_obj_t *text = ui_comp_get_child(item, UI_COMP_SIMPLEITEM_SIMPLE_ITEM_TEXT);
            lv_obj_t *image = ui_comp_get_child(item, UI_COMP_SIMPLEITEM_IMAGE14);

            ESP_LOGI(TAG, "path = %s, name = %s", file_info.path, file_info.name);
            lv_label_set_text(text, file_info.name);
            lv_image_set_src(image, &ui_img_item_music_png);
            
            lv_obj_set_user_data(item, (void *) i);

            file_info_t * info = lv_malloc(sizeof(file_info_t));
            strcpy(info->path, file_info.path);
            strcpy(info->name, file_info.name);
            info->size = file_info.size;

            lv_obj_add_event_cb(item, ui_event_item_cb, LV_EVENT_CLICKED, info);
            lv_obj_add_event_cb(item, ui_event_item_cb, LV_EVENT_DELETE, info);

            if (i == music_index)
            {
                //设置正在播放的歌名
                lv_label_set_text(ui_music_info_label, file_info.name);
                lrc_parse_by_audio_path(file_info.path);
            }

            ui_unlock();
        }
    }

    music_task = NULL;
    vTaskDelete(NULL);
}



void music_loaded(lv_event_t * e)
{
    music_lv_timer = lv_timer_create(refresh_play_info_cb, 1000, NULL);
    lv_timer_pause(music_lv_timer);

    //获取当前播放器信息
    if (player != NULL)
    {
        uint64_t duration = 0;
        esp_player_get_duration(player, &duration);
        uint64_t current_time;
        esp_player_get_play_time(player, &current_time);

        esp_player_state_t state;
        esp_player_get_state(player, &state);

        lv_label_set_text(ui_music_cur_time, FORMAT_SECONDS(current_time / 1000));
        lv_label_set_text(ui_music_duration, FORMAT_SECONDS(duration / 1000));
        lv_slider_set_max_value(ui_music_seek, duration / 1000);
        lv_slider_set_value(ui_music_seek, current_time / 1000, LV_ANIM_OFF);
        if (ESP_PLAYER_STATE_PLAYING == state) {
            switch_ui_pause(NULL);
            lv_timer_resume(music_lv_timer);
        } else {
            switch_ui_play(NULL);
        }
    } 
    else
    {
        player_init(&player);
        esp_player_set_event_cb(player, player_event_cb, NULL);
    }

    if (handle == NULL)
    {
        xTaskCreate(vMusicScannTask, "music scann", 8192, NULL, 5, &music_task);
    }
}

void music_unloaded(lv_event_t * e)
{
    lv_timer_delete(music_lv_timer);
    music_lv_timer = NULL;
}

void music_seek_released(lv_event_t * e)
{
    int v = lv_slider_get_value(ui_music_seek);
    esp_player_seek(player, v * 1000);
}

void music_seek_pressed(lv_event_t * e)
{
    ESP_LOGI(TAG, "music_seek_pressed");
    lv_timer_pause(music_lv_timer);
}

void music_seek_value_changed(lv_event_t * e)
{
    int v = lv_slider_get_value(ui_music_seek);
    lv_label_set_text(ui_music_cur_time, FORMAT_SECONDS(v));
}

void music_action(lv_event_t * e)
{
    lv_obj_t *target = lv_event_get_target(e);

    if (target == ui_play) {

        esp_player_state_t state;
        esp_player_get_state(player, &state);

        if (state == ESP_PLAYER_STATE_PAUSED) {
            esp_player_resume(player);
        } else {
            file_info_t info;
            if (file_list_get_by_index(handle, music_index, &info) == 0)
            {
                esp_player_data_src_t src = ESP_PLAYER_DATA_SRC(info.path, ESP_PLAYER_MASK_AUDIO);
                esp_player_set_data_src(player, &src);
                esp_player_run(player);

                //设置正在播放的歌名
                lv_label_set_text(ui_music_info_label, info.name);
                lrc_parse_by_audio_path(info.path);
            }
        }
    }
    else if (target == ui_pause)
    {
        esp_player_pause(player);
    }
    else if (target == ui_skip_forward)
    {
        next_play_music(NULL);
    }
    else if (target == ui_skip_back)
    {
        previous_play_music(NULL);
    }
}

void music_list_refresh(lv_event_t * e)
{
    ESP_LOGI(TAG, "%s", __func__);
    if (music_task == NULL)
    {
        xTaskCreate(vMusicScannTask, "music scann", 8192, NULL, 5, &music_task);
    }
    
}