#include "button.h"
#include "test.h"
#include "../globals.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>

#define MAX_BUTTONS 12
static button_handle_t g_btns[MAX_BUTTONS];
static int g_btn_count = 0;

static const char *TAG = "INPUT";

static void button_event_cb(void *arg, void *data)
{
    button_event_t event = iot_button_get_event(arg);
    // ESP_LOGI(TAG, "%s", iot_button_get_event_str(event));
    bool gaming = nes_key_set((int) data, event);

    if (!gaming)
    {
        if (event == BUTTON_PRESS_DOWN) {
            int num = (int) data;
            switch (num)
            {
            case GPIO_NUM_3:
                {
                    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                    int32_t command = VOLUME_UP;
            
                    xQueueSendFromISR(xGlobalsQueue, &command, &xHigherPriorityTaskWoken);
                }
                break;
            case GPIO_NUM_46:
                {
                    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                    int32_t command = VOLUME_DOWM;
            
                    xQueueSendFromISR(xGlobalsQueue, &command, &xHigherPriorityTaskWoken);
                }
                break;
            default:
                break;
            }
        }
    }
}

static esp_err_t register_button(int gpio_num, button_handle_t *out_btn)
{ 
    // create gpio button
    const button_config_t btn_cfg = {0};
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = gpio_num,
        .active_level = 0,
    };
    button_handle_t gpio_btn = NULL;
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &gpio_btn);
    if(NULL == gpio_btn) {
        ESP_LOGE(TAG, "Button create failed");
        return ret;
    }
    iot_button_register_cb(gpio_btn, BUTTON_PRESS_DOWN, NULL, button_event_cb, (void *) gpio_num);
    iot_button_register_cb(gpio_btn, BUTTON_PRESS_UP, NULL, button_event_cb, (void *) gpio_num);

    *out_btn = gpio_btn;

    return ESP_OK;
}

static void unregister_button(button_handle_t out_btn)
{ 
    iot_button_delete(out_btn);
}

void button_init(void)
{
    gpio_num_t key[] = {GPIO_NUM_15, GPIO_NUM_16, GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_8, GPIO_NUM_9, GPIO_NUM_3, GPIO_NUM_46};
    for (size_t i = 0; i < sizeof(key) / sizeof(key[0]); i++)
    {
        if (!GPIO_IS_VALID_GPIO(key[i])) {
            ESP_LOGE(TAG, "GPIO %d 非法", key[i]);
            continue;
        }
        button_handle_t gpio_btn;
        if(register_button(key[i], &gpio_btn) == ESP_OK)
        {
            g_btns[g_btn_count++] = gpio_btn;
        }
    
    }
    
}

void button_deinit(void)
{ 
    for (size_t i = 0; i < g_btn_count; i++)
    {
        unregister_button(g_btns[i]);
    }
    g_btn_count = 0;
}