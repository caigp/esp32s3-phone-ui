
#include "esp_timer.h"
#include "ui/ui.h"
#include "esp_log.h"
#include "periph_manager.h"
#include "hardware/aht20.h"
#include "player_wrapper.h"
#include "time_format.h"
#include "hardware/bmp280.h"
#include "lv_ui_lock.h"
#include "utils/file_utils.h"
#include "nofrendo.h"
#include "esp_heap_caps.h"
#include "test.h"
#include "media_init.h"
#include "wifi.h"
#include "esp_mac.h"
#include <time.h>

static const char *TAG = "ui_event_handle";

static TaskHandle_t pxCreatedTask = NULL;
static esp_player_handle_t player = NULL;
static env_sensor_t bmp280_handle;
static aht20_dev_handle_t aht20_handle = NULL;
static lv_timer_t *music_lv_timer;
static file_list_handle_t handle;
static file_list_handle_t game_handle;
static lv_obj_t * file_explorer;

static int music_index = 0;

static char *argv[1];

static TaskHandle_t wifi_wait_TaskHandle = NULL;

static void ui_event_game_item_cb(lv_event_t * e)
{
    lv_event_code_t event_code = lv_event_get_code(e);
    file_info_t * file_info = (file_info_t *)lv_event_get_user_data(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if(event_code == LV_EVENT_CLICKED) {
        argv[0] = file_info->path;
        _ui_screen_change(&ui_gaming, LV_SCR_LOAD_ANIM_OVER_LEFT, 300, 0, &ui_gaming_screen_init);
    }
    else if (event_code == LV_EVENT_DELETE)
    {
        if (file_info)
        {
            lv_free(file_info);
        }
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

static TaskHandle_t music_task = NULL;

static void vMusicScannTask(void *arg)
{
    ui_lock();
    lv_obj_clean(ui_Container45);
    ui_unlock();

    ext_filter_t filters[] = {
        {"mp3", 1},
    };

    char buf[20];
    snprintf(buf, sizeof(buf), "%s/Music", SD_MOUNT_PATH);

    if (handle != NULL)
    {
        file_list_rescan_with_filters(handle, 0, filters, 1, 0);
    }
    else
    {
        handle = file_list_create_ex(buf, 0, filters, 1, 0);
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

            lv_obj_set_y(item, i * 50);
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
            }

            ui_unlock();
        }
    }

    music_task = NULL;
    vTaskDelete(NULL);
}

static TaskHandle_t game_task = NULL;

static void vGameScannTask(void *arg)
{
    ui_lock();
    lv_obj_clean(ui_game_list);
    ui_unlock();

    ext_filter_t filters[] = {
        {"nes", 1},
    };

    char buf[20];
    snprintf(buf, sizeof(buf), "%s/Game", SD_MOUNT_PATH);
    if (game_handle != NULL)
    {
        file_list_rescan_with_filters(game_handle, 0, filters, 1, 0);
    }
    else
    {
        game_handle = file_list_create_ex(buf, 0, filters, 1, 0);
    }
    file_list_print_all(game_handle);

    int32_t count = file_list_get_count(game_handle);
    ESP_LOGI(TAG, "game count %d", count);
    for (int32_t i = 0; i < count; i++)
    {
        file_info_t file_info;
        if (file_list_get_by_index(game_handle, i, &file_info) == 0)
        {
            ui_lock();

            lv_obj_t *item = ui_simpleitem_create(ui_game_list);
            lv_obj_t *text = ui_comp_get_child(item, UI_COMP_SIMPLEITEM_SIMPLE_ITEM_TEXT);
            lv_obj_t *image = ui_comp_get_child(item, UI_COMP_SIMPLEITEM_IMAGE14);

            ESP_LOGI(TAG, "path = %s, name = %s", file_info.path, file_info.name);
            lv_label_set_text(text, file_info.name);
            lv_obj_set_style_text_color(text, lv_color_hex(0x000000), LV_PART_MAIN | LV_STATE_DEFAULT);
            
            lv_image_set_src(image, &ui_img_item_game_png);

            lv_obj_set_y(item, i * 50);
            lv_obj_set_user_data(item, (void *) i);

            file_info_t * info = lv_malloc(sizeof(file_info_t));
            strcpy(info->path, file_info.path);
            strcpy(info->name, file_info.name);
            info->size = file_info.size;


            lv_obj_add_event_cb(item, ui_event_game_item_cb, LV_EVENT_CLICKED, info);
            lv_obj_add_event_cb(item, ui_event_game_item_cb, LV_EVENT_DELETE, info);

            ui_unlock();

            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    game_task = NULL;
    vTaskDelete(NULL);
}

void switch_ui_pause(void *data)
{
    lv_obj_remove_flag(ui_pause, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_play, LV_OBJ_FLAG_HIDDEN);
}

void switch_ui_play(void *data)
{
    lv_obj_remove_flag(ui_play, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ui_pause, LV_OBJ_FLAG_HIDDEN);
}

static void refresh_play_info_cb(lv_timer_t * timer)
{
    uint64_t current_time;
    esp_player_get_play_time(player, &current_time);

    lv_label_set_text(ui_music_cur_time, FORMAT_SECONDS(current_time / 1000));
    lv_slider_set_value(ui_music_seek, current_time / 1000, LV_ANIM_OFF);
}

static void load_launcher(lv_timer_t *) {
    _ui_screen_change(&ui_launcher, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_launcher_screen_init);

    init_status_bar();
    init_navigation_bar();
    init_notification_panel();
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
    }
}

static esp_player_err_t player_event_cb(esp_player_event_msg_t *msg, void *ctx)
{
    ESP_LOGI(TAG, "player_event %d", msg->event_type);
    switch (msg->event_type) {
        case ESP_PLAYER_EVENT_PAUSED:
            lv_async_call(switch_ui_play, NULL);
            break;
        case ESP_PLAYER_EVENT_PLAYED:
            lv_async_call(switch_ui_pause, NULL);
            break;
        case ESP_PLAYER_EVENT_FINISHED:
            lv_async_call(switch_ui_play, NULL);
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
            
            break;
        default:
            break;
    }
    return ESP_PLAYER_ERR_OK;
}

static void read_temp_RH(void *arg)
{
    float temp, hum;
    while (1)
    {
        esp_err_t ret = aht20_read_float(aht20_handle, &temp, &hum);
        if (ret != ESP_OK)
        {
            temp = 0;
            hum = 0;
        }
        float pressure;
        ret = bmp280_read(bmp280_handle.bmp280, &bmp280_handle, &pressure);
        if (ret != ESP_OK)
        {
            pressure = 0;
        }

        ESP_LOGI(TAG, "Humidity      : %2.2f %%", hum);
        ESP_LOGI(TAG, "Temperature   : %2.2f degC", temp);
        ESP_LOGI(TAG, "air pressure   : %2.2f hPa", pressure);

        char buffer1[24];
        char buffer2[24];
        char buffer3[24];
        snprintf(buffer1, sizeof(buffer1), "%s %2.2f℃", "温度", temp);
        snprintf(buffer2, sizeof(buffer2), "%s %2.2f%%", "湿度", hum);
        snprintf(buffer3, sizeof(buffer3), "%2.1f hPa", pressure);

        char temp_tip[40];
        if (temp >= 35)
        {
            snprintf(temp_tip, sizeof(temp_tip), "%s", "注意防暑，减少外出");
        }
        else if (temp >= 30 && temp < 35)
        {
            snprintf(temp_tip, sizeof(temp_tip), "%s", "炎热，注意补水和防晒");
        }
        else if (temp >= 25 && temp < 30)
        {
            snprintf(temp_tip, sizeof(temp_tip), "%s", "温暖，适宜户外活动");
        }
        else if (temp >= 20 && temp < 25)
        {
            snprintf(temp_tip, sizeof(temp_tip), "%s", "舒适，体感宜人");
        }
        else if (temp >= 15 && temp < 20)
        {
            snprintf(temp_tip, sizeof(temp_tip), "%s", "微凉，建议薄外套");
        }
        else if (temp >= 10 && temp < 15)
        {
            snprintf(temp_tip, sizeof(temp_tip), "%s", "偏凉，注意添衣");
        }
        else if (temp >= 5 && temp < 10)
        {
            snprintf(temp_tip, sizeof(temp_tip), "%s", "较冷，注意保暖");
        }
        else if (temp >= 0 && temp < 5)
        {
            snprintf(temp_tip, sizeof(temp_tip), "%s", "寒冷，加强保暖");
        }
        else
        {
            snprintf(temp_tip, sizeof(temp_tip), "%s", "严寒，谨防冻伤");
        }

        char RH_tip[40];
        if (hum >= 80)
        {
            snprintf(RH_tip, sizeof(RH_tip), "%s", "非常潮湿，注意除湿防霉");
        }
        else if (hum >= 70 && hum < 80)
        {
            snprintf(RH_tip, sizeof(RH_tip), "%s", "潮湿，体感偏闷");
        }
        else if (hum >= 60 && hum < 70)
        {
            snprintf(RH_tip, sizeof(RH_tip), "%s", "微潮，体感较舒适");
        }
        else if (hum >= 40 && hum < 60)
        {
            snprintf(RH_tip, sizeof(RH_tip), "%s", "湿度适中，最舒适");
        }
        else if (hum >= 30 && hum < 40)
        {
            snprintf(RH_tip, sizeof(RH_tip), "%s", "偏干，注意补水保湿");
        }
        else
        {
            snprintf(RH_tip, sizeof(RH_tip), "%s", "非常干燥，注意保湿防静电");
        }
        
        ui_lock();

        lv_label_set_text(ui_temp_text, buffer1);
        lv_label_set_text(ui_RH_text, buffer2);
        lv_label_set_text(ui_hPa_text, buffer3);
        lv_arc_set_value(ui_temp_Arc, temp);
        lv_arc_set_value(ui_RH_Arc, hum);
        lv_arc_set_value(ui_hPa_Arc1, pressure);
        lv_label_set_text(ui_temp_tip, temp_tip);
        lv_label_set_text(ui_RH_tip, RH_tip);
        
        ui_unlock();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
}

void cal_ret(lv_event_t * e)
{
	// Your code here
}

void start_boot(lv_event_t * e)
{
	// Your code here
}

static void toast_timer_cb(lv_timer_t *)
{
    lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(toast_timer);
}

void bootinit(lv_event_t * e)
{
	lv_obj_set_style_anim_time(ui_bootbar, 2000, LV_PART_MAIN);
    lv_bar_set_value(ui_bootbar, 80, LV_ANIM_ON);

    lv_obj_t * obj_t = lv_layer_top();
    //音量条
    volume_slider = ui_volumeslider_create(obj_t);
    lv_slider_set_value(volume_slider, sys_config.volume, LV_ANIM_OFF);
    //toast
    toast = ui_toast_create(obj_t);
    lv_obj_set_style_text_font(toast, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);
    toast_timer = lv_timer_create(toast_timer_cb, 3000, NULL);
    lv_timer_pause(toast_timer);

    lv_timer_set_repeat_count(lv_timer_create(load_launcher, 2100, NULL), 1);
}

void temp_RH_load(lv_event_t * e)
{
	ESP_LOGI(TAG, "_temp_RH_load");

    lv_obj_remove_style(ui_temp_Arc, NULL, LV_PART_KNOB);
    lv_obj_remove_style(ui_RH_Arc, NULL, LV_PART_KNOB);
    lv_obj_remove_style(ui_hPa_Arc1, NULL, LV_PART_KNOB);

    lv_arc_set_min_value(ui_temp_Arc, -40);
    lv_arc_set_max_value(ui_temp_Arc, 80);
    lv_arc_set_min_value(ui_hPa_Arc1, 300);
    lv_arc_set_max_value(ui_hPa_Arc1, 1100);


    // i2c_aht20_config_t aht20_i2c_config = {
    //     .i2c_config.device_address = AHT20_ADDRESS_0,
    //     .i2c_config.scl_speed_hz = I2C_MASTER_FREQ_HZ,
    //     .i2c_timeout = 100,
    // };
    // aht20_new_sensor(g_i2c_bus, &aht20_i2c_config, &aht20_handle);

    // bmp280_new_sensor(g_i2c_bus, &bmp280_handle);

    // BaseType_t xReturned = xTaskCreate(read_temp_RH, "TempRHTask", 4096, NULL, 1, &pxCreatedTask);
    // if (xReturned == pdPASS)
    // {
    //     ESP_LOGI(TAG, "TempRHTask 创建成功");
    // }
    
}

void temp_RH_unload(lv_event_t * e)
{
	ESP_LOGI(TAG, "_temp_RH_unload");

    // aht20_del_sensor(&aht20_handle);
    // aht20_handle = NULL;
    // bmp280_del_sensor(&bmp280_handle);
    // if (pxCreatedTask)
    // {
    //     vTaskDelete(pxCreatedTask);
    // }
    // pxCreatedTask = NULL;
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

void music_loaded(lv_event_t * e)
{
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
        } else {
            switch_ui_play(NULL);
        }
    } 
    else
    {
        player_init(&player);
        esp_player_set_event_cb(player, player_event_cb, NULL);
    }
    
    music_lv_timer = lv_timer_create(refresh_play_info_cb, 1000, NULL);

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
    lv_timer_resume(music_lv_timer);
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

int i = 11;
int last_first_index = 0;

void scroll_cb(lv_event_t *e) {
    lv_obj_t *container = lv_event_get_target(e);
    
    // 获取当前滚动偏移量
    int scroll_y = lv_obj_get_scroll_y(container);
    
    ESP_LOGI(TAG, "scroll_y = %d" , scroll_y);
    // 计算当前应该显示的第一行索引
     int first_index = scroll_y / 50;
    ESP_LOGI(TAG, "first_index = %d" , first_index);
    // 更新可见行内容
    // update_visible_rows(first_index);

    if (first_index > last_first_index)
    {
        
        // lv_obj_t *item = ui_simpleitem_create(ui_Container49);
        //     lv_obj_t *text = ui_comp_get_child(item, UI_COMP_SIMPLEITEM_SIMPLE_ITEM_TEXT);

        //     lv_label_set_text(text, "测试列表");

        // lv_obj_set_y(item, i * 50);

        // lv_obj_update_layout(ui_Container49);

        // lv_obj_move_children_by()
        // lv_obj_update_layout(ui_Container49);
        
        // i++;
        // lv_obj_set_user_data(item, (void *) i);
        // lv_obj_delete(rows[0]);
        // lv_obj_delete(rows[1]);
        // lv_obj_delete(rows[2]);
        // lv_obj_delete(rows[3]);
        // lv_obj_delete(rows[4]);
        // lv_obj_delete(rows[5]);

        last_first_index = first_index;
        
    }
}


void game_loaded(lv_event_t * e)
{
    if (game_handle == NULL)
    {
        xTaskCreate(vGameScannTask, "game scann", 8192, NULL, 5, &game_task);
    }
}



static lv_img_dsc_t nes_img_dsc;
static void nes_lvgl_update_cb(void *data) {

    // 更新数据指针
    nes_img_dsc.data = (const uint8_t *)data;
    
    // 刷新图像
    lv_img_set_src(ui_game_video, &nes_img_dsc);
    lv_obj_invalidate(ui_game_video);
}

// NES 视频回调 (在 Core 1 被调用)
void nes_video_callback(void *data) {
    ui_lock();
    nes_lvgl_update_cb(data);
    ui_unlock();
}

// NES 模拟任务
static void nes_simulation_task(void *arg) {
    ESP_LOGI("NES", "Task started on Core %d", xPortGetCoreID());
    nofrendo_main(0, argv);
    ESP_LOGI("NES", "Task stopped");
    vTaskDelete(NULL);
}

static esp_audio_render_stream_handle_t stream = NULL;

// 页面加载事件
void gaming_loaded(lv_event_t * e) {

    xTaskCreatePinnedToCore(nes_simulation_task, "NES_Task", 8192, NULL, 5, NULL, 1);
    ESP_LOGI("NES", "Task created");
}

void gaming_unloaded(lv_event_t * e)
{
    main_quit();
}

void nes_audio_callback(const void *src, size_t size)
{
    if (stream != NULL)
    {
        esp_audio_render_stream_write(stream, (uint8_t *) src, size);
    }
}

void nes_audio_init()
{
    uint8_t stream_id = 1;
    
    if (esp_audio_render_stream_get(s_render, ESP_AUDIO_RENDER_STREAM_ID(stream_id),
                                    &stream) != ESP_AUDIO_RENDER_ERR_OK) {
        ESP_LOGE(TAG, "Failed to get render stream %u", stream_id);
        // return ESP_PLAYER_ERR_FAIL;
    }

        // 假设输入采样信息为 16kHz、2ch、16bit
    esp_audio_render_sample_info_t in = {
        .sample_rate = 44100,
        .channel = 1,
        .bits_per_sample = 16,
    };
    // esp_audio_render_stream_handle_t stream;
    esp_audio_render_err_t err = esp_audio_render_stream_open(stream, &in);
    ESP_LOGI(TAG, "esp_audio_render_stream_open returned = %d", err);
}

void nes_audio_deinit()
{
    esp_audio_render_stream_close(stream);
    stream = NULL;
}

void nes_img_set(int pitch, int height)
{
    nes_img_dsc.header.w = pitch;
    nes_img_dsc.header.h = height;
    nes_img_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    nes_img_dsc.data_size = pitch * height * 2;
}

void nes_video_init()
{ 
    ui_lock();
    
    lv_obj_set_style_bg_opa(ui_game_video, LV_OPA_TRANSP, 0);
    lv_image_set_inner_align(ui_game_video, LV_IMAGE_ALIGN_CONTAIN);


    // lv_display_set_rotation(lv_disp_get_default(), LV_DISP_ROTATION_270);

    ui_unlock();
}
void nes_video_deinit()
{
    ui_lock();
    lv_img_set_src(ui_game_video, NULL);
    lv_obj_invalidate(ui_game_video);
    ui_unlock();
}

void gaming_paused(lv_event_t * e)
{
    nes_toggle_pause();
    lv_obj_remove_flag(ui_Container50, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_event_cb(ui_gaming, ui_event_gaming);
}

void gaming_resume(lv_event_t * e)
{
    nes_toggle_pause();
    lv_obj_add_flag(ui_Container50, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(ui_gaming, ui_event_gaming, LV_EVENT_ALL, NULL);
}

void gaming_exit(lv_event_t * e)
{
    main_quit();
    lv_obj_add_event_cb(ui_gaming, ui_event_gaming, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(ui_Container50, LV_OBJ_FLAG_HIDDEN);
    _ui_screen_change(&ui_nes_game, LV_SCR_LOAD_ANIM_OUT_RIGHT, 300, 0, &ui_launcher_screen_init);
}

static void fe_event_cb(lv_event_t * e)
{

    lv_obj_t * fe = lv_event_get_target(e);   // 文件浏览器对象本身

    const char * cur_path = lv_file_explorer_get_current_path(fe);
    const char * sel_fn   = lv_file_explorer_get_selected_file_name(fe);

    lv_obj_remove_flag(ui_Container54, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ui_Container54);


    lv_label_set_text(ui_file_name, sel_fn);
    lv_obj_set_user_data(ui_Container54, (void *) strdup(cur_path));
    lv_obj_set_user_data(ui_Label40, (void *) strdup(sel_fn));
}

void file_manager_loaded(lv_event_t * e)
{
    if (file_explorer == NULL)
    {
        file_explorer = lv_file_explorer_create(ui_file_manager);
        lv_file_explorer_open_dir(file_explorer, "A:/");

        // lv_obj_t *quick_access_area = lv_file_explorer_get_quick_access_area(file_explorer);
        // lv_obj_set_width(quick_access_area, lv_pct(50));

        lv_obj_t *path_label = lv_file_explorer_get_path_label(file_explorer);
        lv_obj_set_style_text_font(path_label, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *file_table = lv_file_explorer_get_file_table(file_explorer);
        lv_obj_set_style_text_font(file_table, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_top(file_table, 6, LV_PART_ITEMS);
        lv_obj_set_style_pad_bottom(file_table, 6, LV_PART_ITEMS);

        lv_obj_add_event_cb(file_explorer, fe_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }
}

void file_manager_unloaded(lv_event_t * e)
{
    if (file_explorer != NULL)
    {
        lv_obj_del(file_explorer);
        file_explorer = NULL;
    }
}

void volume_change(lv_event_t * e)
{
    lv_obj_t * obj = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(obj);
    set_volume(val);
    global_media_update_volume(false);
    lv_timer_reset(hide_timer);
}

void change_game_sound(lv_event_t * e) 
{
    ESP_LOGI(TAG, "%s", __func__);
    if(nes_toggle_sound())
    {
        lv_label_set_text(ui_game_sound_label, "关闭声音");
    }
    else
    {
        lv_label_set_text(ui_game_sound_label, "开启声音");
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

void game_list_refresh(lv_event_t * e)
{
    if (game_task == NULL)
    {
        xTaskCreate(vGameScannTask, "game scann", 8192, NULL, 5, &game_task);
    }
}

void file_del(lv_event_t * e)
{
    char *cur_path = (char *) lv_obj_get_user_data(ui_Container54);
    ESP_LOGI(TAG, "cur_path = %s", cur_path);

    char *sel_fn = (char *) lv_obj_get_user_data(ui_Label40);
    ESP_LOGI(TAG, "sel_fn = %s", sel_fn);

    char full[256];
    snprintf(full, sizeof(full), "%s%s%s", SD_MOUNT_PATH, cur_path + 2, sel_fn);

    ESP_LOGI(TAG, "full path = %s", full);

    if(!remove(full))
    {
        show_toast("已删除");
        lv_file_explorer_open_dir(file_explorer, cur_path);
    }
    else
    {
        show_toast("删除失败");
    }
    

    free(cur_path);
    free(sel_fn);

    lv_obj_add_flag(ui_Container54, LV_OBJ_FLAG_HIDDEN);
}

void file_act_close(lv_event_t * e)
{
    lv_obj_add_flag(ui_Container54, LV_OBJ_FLAG_HIDDEN);
}

static void event_cb(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target_obj(e);
    lv_obj_t * label = lv_obj_get_child(btn, 0);
    LV_UNUSED(label);
    LV_LOG_USER("Button %s clicked", lv_label_get_text(label));
}



void wifi_loaded(lv_event_t * e)
{ 
    xTaskNotifyGive(wifi_scan_task);
}

void wifi_unloaded(lv_event_t * e){

}

void wifi_wait_task(void *pvParameters)
{ 
    xEventGroupClearBits(xWifiEventGroup, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    EventBits_t uxBits = xEventGroupWaitBits(
        xWifiEventGroup,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,   // 任一置位即返回
        portMAX_DELAY
    );
    ESP_LOGI(TAG, "%s", __func__);

    ui_lock();
    if (uxBits & WIFI_CONNECTED_BIT) {
        show_toast("WIFI 连接成功");
        ESP_LOGI(TAG, "WiFi connected");
        xEventGroupClearBits(xWifiEventGroup, WIFI_CONNECTED_BIT);
        
    }
    if (uxBits & WIFI_FAIL_BIT) {
        show_toast("WIFI 连接失败");
        ESP_LOGI(TAG, "WiFi connect failed");
        xEventGroupClearBits(xWifiEventGroup, WIFI_FAIL_BIT);
        
    }
    ui_unlock();


    wifi_wait_TaskHandle = NULL;
    vTaskDelete(NULL);
}

void wifi_pwd_done(lv_event_t * e)
{ 
    ESP_LOGI(TAG, "%s", __func__);
    // show_toast("请稍后...");

    if (wifi_wait_TaskHandle == NULL)
    {
        xTaskCreate(wifi_wait_task, "wifi wait", 4096, NULL, 5, &wifi_wait_TaskHandle);
    }

    const char *password = lv_textarea_get_text(ui_wifi_pwd_TextArea);
    wifi_ap_record_t *ap_record = lv_obj_get_user_data(ui_wifi_input_pwd);
    wifi_connect(ap_record, password);
}

void on_show_password_changed(lv_event_t * e)
{
    lv_obj_t * cb = lv_event_get_target_obj(e);

    bool checked = lv_obj_has_state(cb, LV_STATE_CHECKED);
    lv_textarea_set_password_mode(ui_wifi_pwd_TextArea, !checked);
}

void wifi_input_pwd_unloaded(lv_event_t * e)
{
    wifi_ap_record_t *ap_record = lv_obj_get_user_data(ui_wifi_input_pwd);
    ESP_LOGI(TAG, "%s %p", __func__, ap_record);
    if (ap_record)
    {
        lv_free(ap_record);
    }
}

void calendar_loaded(lv_event_t * e)
{
    uint32_t year = timeinfo.tm_year + 1900;
    uint32_t month = timeinfo.tm_mon + 1;
    uint32_t day = timeinfo.tm_mday;

    lv_calendar_set_today_year(ui_Calendar_widget, year);
    lv_calendar_set_today_month(ui_Calendar_widget, month);
    lv_calendar_set_today_day(ui_Calendar_widget, day);
    lv_calendar_set_shown_year(ui_Calendar_widget, year);
    lv_calendar_set_shown_month(ui_Calendar_widget, month);
    lv_calendar_set_chinese_mode(ui_Calendar_widget, true);
    
    static const char * day_names[7] = {"日", "一", "二", "三", "四", "五", "六"};
    lv_calendar_set_day_names(ui_Calendar_widget, day_names);


    static lv_calendar_date_t highlighted_days[3];       /*Only its pointer will be saved so should be static*/
    highlighted_days[0].year = year;
    highlighted_days[0].month = month;
    highlighted_days[0].day = day;
    lv_calendar_set_highlighted_dates(ui_Calendar_widget, highlighted_days, 1);

}