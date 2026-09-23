#include "media_init.h"
#include "ui/ui.h"

static const char *TAG = "media_init";

static const audio_codec_data_if_t *data_if = NULL;
static esp_gmf_pool_handle_t s_pool = NULL;

esp_codec_dev_handle_t codec_dev = NULL;
esp_audio_render_handle_t s_render = NULL;

lv_obj_t * volume_slider;
lv_timer_t * hide_timer;

static void destroy_audio_render(void)
{
    if (s_render) {
        esp_audio_render_destroy(s_render);
        s_render = NULL;
    }
    if (s_pool) {
        esp_gmf_pool_deinit(s_pool);
        s_pool = NULL;
    }
}

static int register_render_pool(esp_gmf_pool_handle_t *out_pool)
{
    *out_pool = NULL;
    if (esp_gmf_pool_init(out_pool) != ESP_GMF_ERR_OK) {
        return -1;
    }
    esp_gmf_element_handle_t el = NULL;

    esp_ae_ch_cvt_cfg_t ch_cfg = DEFAULT_ESP_GMF_CH_CVT_CONFIG();
    if (esp_gmf_ch_cvt_init(&ch_cfg, &el) == ESP_GMF_ERR_OK) {
        esp_gmf_pool_register_element(*out_pool, el, NULL);
    }

    esp_ae_bit_cvt_cfg_t bit_cfg = DEFAULT_ESP_GMF_BIT_CVT_CONFIG();
    if (esp_gmf_bit_cvt_init(&bit_cfg, &el) == ESP_GMF_ERR_OK) {
        esp_gmf_pool_register_element(*out_pool, el, NULL);
    }

    esp_ae_rate_cvt_cfg_t rate_cfg = DEFAULT_ESP_GMF_RATE_CVT_CONFIG();
    if (esp_gmf_rate_cvt_init(&rate_cfg, &el) == ESP_GMF_ERR_OK) {
        esp_gmf_pool_register_element(*out_pool, el, NULL);
    }

    esp_ae_alc_cfg_t alc_cfg = DEFAULT_ESP_GMF_ALC_CONFIG();
    if (esp_gmf_alc_init(&alc_cfg, &el) == ESP_GMF_ERR_OK) {
        esp_gmf_pool_register_element(*out_pool, el, NULL);
    }
    return 0;
}

static void register_media_defaults(void)
{
    media_lib_add_default_adapter();
    esp_extractor_register_default();
    esp_audio_dec_register_default();
}

static void unregister_media_defaults(void)
{
    esp_audio_dec_unregister_default();
    esp_extractor_unregister_default();
}

void esp_codec_dev_init(void)
{
    audio_codec_i2s_cfg_t i2s_cfg = {
        .port = I2S_NUM_0,
        .rx_handle = rx_chan,
        .tx_handle = tx_chan,
    };
    data_if = audio_codec_new_i2s_data(&i2s_cfg);

    esp_codec_dev_cfg_t dev_cfg = {
        .codec_if = NULL,
        .data_if = data_if,
        .dev_type = ESP_CODEC_DEV_TYPE_IN_OUT,
    };
    codec_dev = esp_codec_dev_new(&dev_cfg);

    esp_codec_dev_sample_info_t fs = {
        .sample_rate = SAMPLE_RATE,
        .channel = CHANNEL,
        .bits_per_sample = SLOT_BITS,
    };
    esp_codec_dev_open(codec_dev, &fs);
    static esp_codec_dev_vol_map_t volume_maps[] = {
        {.vol = 0, .db_value = -100},
        {.vol = 10, .db_value = -46},
        {.vol = 20, .db_value = -22},
        {.vol = 30, .db_value = -21},
        {.vol = 40, .db_value = -20},
        {.vol = 50, .db_value = -19},
        {.vol = 60, .db_value = -18},
        {.vol = 70, .db_value = -17},
        {.vol = 80, .db_value = -16},
        {.vol = 90, .db_value = -6},
        {.vol = 100, .db_value = 0.0},
    };
        esp_codec_dev_vol_curve_t vol_curve = {
        .count = sizeof(volume_maps) / sizeof(esp_codec_dev_vol_map_t),
        .vol_map = volume_maps,
    };
    int ret = esp_codec_dev_set_vol_curve(codec_dev, &vol_curve);
    ESP_LOGI(TAG, "esp_codec_dev_set_vol_curve = %d", ret);
    esp_codec_dev_set_out_vol(codec_dev, sys_config.volume);
}

void esp_codec_dev_deinit(void)
{
    esp_codec_dev_close(codec_dev);
    esp_codec_dev_delete(codec_dev);
    if (data_if != NULL)
    {
        audio_codec_delete_data_if(data_if);
        data_if = NULL;
    }
    
}

static int render_writer_cb(uint8_t *pcm, uint32_t len, void *ctx)
{

    esp_codec_dev_handle_t dev = (esp_codec_dev_handle_t)ctx;
    if (dev == NULL || pcm == NULL || len == 0) {
        return -1;
    }
    // memset(pcm, 0, len);
    return esp_codec_dev_write(dev, pcm, len);
}

static esp_audio_render_err_t create_audio_render(void)
{
    if (register_render_pool(&s_pool) != 0) {
        ESP_LOGE(TAG, "Failed to register GMF audio processing pool");
        goto fail;
    }

    esp_audio_render_cfg_t cfg = {
        .max_stream_num = 2,
        .out_writer = render_writer_cb,
        .out_ctx = codec_dev,
        .out_sample_info = {
            .sample_rate = SAMPLE_RATE,
            .bits_per_sample = SLOT_BITS,
            .channel = CHANNEL,
        },
        .pool = s_pool,
        .process_period = 20,
    };

    if (esp_audio_render_create(&cfg, &s_render) != ESP_AUDIO_RENDER_ERR_OK) {
        ESP_LOGE(TAG, "Failed to create esp_audio_render");
        goto fail;
    }

    return ESP_AUDIO_RENDER_ERR_OK;

fail:
    destroy_audio_render();
    return ESP_AUDIO_RENDER_ERR_FAIL;
}

void global_media_init()
{
    register_media_defaults();
    esp_codec_dev_init();
    create_audio_render();
}

void global_media_deinit()
{
    destroy_audio_render();
    esp_codec_dev_deinit();
    unregister_media_defaults();
}

void hide_timer_cb(lv_timer_t *user_data)
{
    lv_obj_add_flag(volume_slider, LV_OBJ_FLAG_HIDDEN);
    lv_timer_pause(hide_timer);
}

/* 包装函数：缩放 */
static void anim_scale_cb(void *obj, int32_t value)
{
    lv_obj_set_style_transform_scale_x((lv_obj_t*)obj, value, 0);
    lv_obj_set_style_transform_scale_y((lv_obj_t*)obj, value, 0);
}

void async_xcb(void *user_data)
{
    bool hidden_flag = lv_obj_has_flag(volume_slider, LV_OBJ_FLAG_HIDDEN);

    lv_slider_set_value(volume_slider, sys_config.volume, LV_ANIM_ON);

    if (hidden_flag)
    {    
        lv_obj_remove_flag(volume_slider, LV_OBJ_FLAG_HIDDEN);
        
        /* 设置变换中心点为对象中心 */
        lv_obj_set_style_transform_pivot_x(volume_slider, lv_obj_get_width(volume_slider) / 2, 0);
        lv_obj_set_style_transform_pivot_y(volume_slider, lv_obj_get_height(volume_slider) / 2, 0);

        /* 初始状态：缩小到 0 */
        lv_obj_set_style_transform_scale_x(volume_slider, 0, 0);
        lv_obj_set_style_transform_scale_y(volume_slider, 0, 0);

        /* 缩放动画 */
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, volume_slider);
        lv_anim_set_exec_cb(&a, anim_scale_cb);
        lv_anim_set_values(&a, 0, 256);  /* 256 = 100% */
        lv_anim_set_time(&a, 200);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_start(&a);
    }

    if (hide_timer == NULL)
    {
        hide_timer = lv_timer_create(hide_timer_cb, 3000, NULL);
    }
    else
    {
        lv_timer_reset(hide_timer);
        lv_timer_resume(hide_timer);
    }
}


/* 更新全局的音量 */
void global_media_update_volume(bool handle_sync)
{
    // ESP_LOGI(TAG, "sys_config.volume = %d", sys_config.volume);
    esp_codec_dev_set_out_vol(codec_dev, sys_config.volume);
    // bool mute = false;
    // esp_codec_dev_get_out_mute(codec_dev, &mute);
    // esp_codec_dev_set_out_mute(codec_dev, (!mute));
    if (handle_sync)
    {
        lv_async_call(async_xcb, NULL);
    }
    
}