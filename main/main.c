#include <stdio.h>
#include <unistd.h>
#include <sys/lock.h>
#include <sys/param.h>
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"

#include "lvgl.h"
#include "lv_examples.h"
#include "ui/ui.h"

#include "board.h"
#include "periph_manager.h"
#include "media_init.h"
#include "lv_ui_lock.h"
#include "nofrendo.h"
#include "input/button.h"
#include "globals.h"
#include "wifi.h"
#include "nvs_flash.h"
#include "lv_demos.h"
#include "sd_file_cache.h"
#include "wav_header.h"
#include "esp_coze_chat.h"

static const char *TAG = "MAIN";

#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS  500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS  1
#define EXAMPLE_LVGL_TASK_STACK_SIZE    8192
#define EXAMPLE_LVGL_TASK_PRIORITY      5

static SemaphoreHandle_t lvgl_flush_done;

extern void example_lvgl_demo_ui(lv_display_t *disp);

/* Apply the panel orientation only at initialization or a rotation change. */
static void example_lvgl_port_update_callback(lv_display_t *disp)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    lv_display_rotation_t rotation = lv_display_get_rotation(disp);

    switch (rotation) {
    case LV_DISPLAY_ROTATION_0:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, true, false);
        break;
    case LV_DISPLAY_ROTATION_90:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, true, true);
        break;
    case LV_DISPLAY_ROTATION_180:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, false);
        esp_lcd_panel_mirror(panel_handle, false, true);
        break;
    case LV_DISPLAY_ROTATION_270:
        // Rotate LCD display
        esp_lcd_panel_swap_xy(panel_handle, true);
        esp_lcd_panel_mirror(panel_handle, false, false);
        break;
    }
}

static void example_lvgl_rotation_cb(lv_event_t *event)
{
    lv_display_t *display = lv_event_get_user_data(event);
    example_lvgl_port_update_callback(display);
}

static void example_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // because SPI LCD is big-endian, we need to swap the RGB bytes order
    lv_draw_sw_rgb565_swap(px_map, (offsetx2 + 1 - offsetx1) * (offsety2 + 1 - offsety1));
    // copy a buffer's content to a specific area of the display
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map));
}

static void example_lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    uint16_t touchpad_x[1] = {0};
    uint16_t touchpad_y[1] = {0};
    uint8_t touchpad_cnt = 0;

    esp_lcd_touch_handle_t touch_pad = lv_indev_get_user_data(indev);
    esp_lcd_touch_read_data(touch_pad);
    /* Get coordinates */
    bool touchpad_pressed = esp_lcd_touch_get_coordinates(touch_pad, touchpad_x, touchpad_y, NULL, &touchpad_cnt, 1);

    if (touchpad_pressed && touchpad_cnt > 0) {
        data->point.x = touchpad_x[0];
        data->point.y = touchpad_y[0];
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static uint32_t example_lvgl_get_tick(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void example_lvgl_wake_task(void *arg)
{
    xTaskNotifyGive((TaskHandle_t)arg);
}

static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    BaseType_t task_woken = pdFALSE;
    xSemaphoreGiveFromISR((SemaphoreHandle_t)user_ctx, &task_woken);
    /* The SPI panel backend does not propagate this callback's return value. */
    if (task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
    return task_woken == pdTRUE;
}

static void example_lvgl_flush_wait_cb(lv_display_t *disp)
{
    /* LVGL clears its flushing flag when this callback returns. */
    xSemaphoreTake(lvgl_flush_done, portMAX_DELAY);
}

static void example_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    /* A one-shot timer avoids rounding every animation deadline to a 10 ms RTOS tick. */
    const esp_timer_create_args_t wake_timer_args = {
        .callback = example_lvgl_wake_task,
        .arg = xTaskGetCurrentTaskHandle(),
        .name = "lvgl_wake",
    };
    esp_timer_handle_t wake_timer;
    ESP_ERROR_CHECK(esp_timer_create(&wake_timer_args, &wake_timer));
    uint32_t time_till_next_ms = 0;
    while (1) {
        // ESP_LOGI(TAG, "lv_timer_handler");
        ui_lock();
        time_till_next_ms = lv_timer_handler();
        ui_unlock();
        // in case of triggering a task watch dog time out
        time_till_next_ms = MAX(time_till_next_ms, EXAMPLE_LVGL_TASK_MIN_DELAY_MS);
        // in case of lvgl display not ready yet
        time_till_next_ms = MIN(time_till_next_ms, EXAMPLE_LVGL_TASK_MAX_DELAY_MS);
        ESP_ERROR_CHECK(esp_timer_start_once(wake_timer, (uint64_t)time_till_next_ms * 1000));
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    }
}

void lvgl_config(lv_display_t **_display)
{
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    lv_tick_set_cb(example_lvgl_get_tick);

    lvgl_flush_done = xSemaphoreCreateBinary();
    assert(lvgl_flush_done);

    // create a lvgl display
    lv_display_t *display = lv_display_create(LCD_H_RES, LCD_V_RES);
    *_display = display;

    // alloc draw buffers used by LVGL
    size_t draw_buffer_sz = LCD_H_RES * LVGL_DRAW_BUF_LINES * sizeof(lv_color16_t);

    void *buf1 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_sz, 0);
    assert(buf1);
    void *buf2 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_sz, 0);
    assert(buf2);

    // void *buf1 = heap_caps_malloc(draw_buffer_sz * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    // void *buf2 = heap_caps_malloc(draw_buffer_sz * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    
    // initialize LVGL draw buffers
    lv_display_set_buffers(display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    // associate the mipi panel handle to the display
    lv_display_set_user_data(display, panel_handle);
    example_lvgl_port_update_callback(display);
    lv_display_add_event_cb(display, example_lvgl_rotation_cb, LV_EVENT_RESOLUTION_CHANGED, display);
    // set color depth
    lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
    // set the callback which can copy the rendered image to an area of the display
    lv_display_set_flush_cb(display, example_lvgl_flush_cb);
    lv_display_set_flush_wait_cb(display, example_lvgl_flush_wait_cb);

    ESP_LOGI(TAG, "Register io panel event callback for LVGL flush ready notification");
    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = example_notify_lvgl_flush_ready,
    };
    /* Register done callback */
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(io_handle, &cbs, lvgl_flush_done));

    static lv_indev_t *indev;
    indev = lv_indev_create(); // Input device driver (Touch)
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, display);
    lv_indev_set_user_data(indev, tp);
    lv_indev_set_read_cb(indev, example_lvgl_touch_cb);
    lv_indev_set_gesture_min_velocity(indev, 1);
    lv_indev_set_gesture_min_distance(indev, 20);

    ESP_LOGI(TAG, "Create LVGL task");
    // 创建 LVGL 任务，绑定到 Core 0
    xTaskCreatePinnedToCore(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL, 0);

    ESP_LOGI(TAG, "Display LVGL Meter Widget");

    
}

/* 开背光 */
void bl_on()
{
    gpio_reset_pin(PIN_NUM_LCD_BL);
    gpio_set_direction(PIN_NUM_LCD_BL, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_LCD_BL, 1);
}


void system_handler_task(void *pvParameter)
{ 
    int32_t received = 0;
    for (;;)
    {
        // 阻塞接收，如果队列空则等待
        if (xQueueReceive(xGlobalsQueue, &received, portMAX_DELAY) == pdPASS)
        {
            // 处理数据
            switch (received)
            {
            case VOLUME_DOWM:
                set_volume(-1);
                global_media_update_volume(true);
                break;
            case VOLUME_UP:
                set_volume(101);
                global_media_update_volume(true);
                break;
            default:
                break;
            }
        }
    }
}

void init_system_handler()
{
    // 创建队列：最多存 5 个 int32_t
    xGlobalsQueue = xQueueCreate(5, sizeof(int32_t));

    if (xGlobalsQueue)
    {
        ESP_LOGI(TAG, "xGlobalsQueue Create");
        xTaskCreate(system_handler_task, "system_handler", 2048, NULL, 5, NULL);
    }
    
}


void app_main(void)
{
    button_init();
    periph_manager_init();
    global_media_init();

    lv_display_t *display = NULL;
    lvgl_config(&display);

    // Lock the mutex due to the LVGL APIs are not thread-safe
    ui_lock();
    // lv_display_set_rotation(display, LV_DISPLAY_ROTATION_90);
    ui_init();
    ui_unlock();

    /* 打开背光 */
    bl_on();

    global_init();
    wifi_init_sta();
    init_system_handler();
    wifi_scan_init();
}
