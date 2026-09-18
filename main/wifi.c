#include "wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "globals.h"
#include "ui/ui.h"
#include "lv_ui_lock.h"

#define DEFAULT_SCAN_LIST_SIZE 16

#ifdef CONFIG_EXAMPLE_USE_SCAN_CHANNEL_BITMAP
#define USE_CHANNEL_BITMAP 1
#define CHANNEL_LIST_SIZE 3
static uint8_t channel_list[CHANNEL_LIST_SIZE] = {1, 6, 11};
#endif /*CONFIG_EXAMPLE_USE_SCAN_CHANNEL_BITMAP*/

static const char *TAG = "wifi";

EventGroupHandle_t xWifiEventGroup;
EventGroupHandle_t xWifiStatGroup;

bool isconnected = false;

TaskHandle_t wifi_scan_task = NULL;

static void print_auth_mode(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_OWE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OWE");
        break;
    case WIFI_AUTH_WEP:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_ENTERPRISE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA3_ENTERPRISE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA2_WPA3_ENTERPRISE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_ENT_192:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_ENT_192");
        break;
    case WIFI_AUTH_DPP:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_DPP");
        break;
    default:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}

static void print_cipher_type(int pairwise_cipher, int group_cipher)
{
    switch (pairwise_cipher) {
        case WIFI_CIPHER_TYPE_NONE:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
            break;
        case WIFI_CIPHER_TYPE_WEP40:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
            break;
        case WIFI_CIPHER_TYPE_WEP104:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
            break;
        case WIFI_CIPHER_TYPE_TKIP:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
            break;
        case WIFI_CIPHER_TYPE_CCMP:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
            break;
        case WIFI_CIPHER_TYPE_TKIP_CCMP:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
            break;
        case WIFI_CIPHER_TYPE_AES_CMAC128:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_AES_CMAC128");
            break;
        case WIFI_CIPHER_TYPE_SMS4:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_SMS4");
            break;
        case WIFI_CIPHER_TYPE_GCMP:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP");
            break;
        case WIFI_CIPHER_TYPE_GCMP256:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_GCMP256");
            break;
        default:
            ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
            break;
    }

    switch (group_cipher) {
        case WIFI_CIPHER_TYPE_NONE:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
            break;
        case WIFI_CIPHER_TYPE_WEP40:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
            break;
        case WIFI_CIPHER_TYPE_WEP104:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
            break;
        case WIFI_CIPHER_TYPE_TKIP:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
            break;
        case WIFI_CIPHER_TYPE_CCMP:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
            break;
        case WIFI_CIPHER_TYPE_TKIP_CCMP:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
            break;
        case WIFI_CIPHER_TYPE_SMS4:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_SMS4");
            break;
        case WIFI_CIPHER_TYPE_GCMP:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP");
            break;
        case WIFI_CIPHER_TYPE_GCMP256:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_GCMP256");
            break;
        default:
            ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
            break;
    }
}

#ifdef USE_CHANNEL_BITMAP
static void array_2_channel_bitmap(const uint8_t channel_list[], const uint8_t channel_list_size, wifi_scan_config_t *scan_config) {

    for(uint8_t i = 0; i < channel_list_size; i++) {
        uint8_t channel = channel_list[i];
        scan_config->channel_bitmap.ghz_2_channels |= (1 << channel);
    }
}
#endif /*USE_CHANNEL_BITMAP*/

void wifi_scan(wifi_ap_record_t *ap_info, uint16_t *number)
{
    ESP_LOGI(TAG, "%s", __func__);

    // EventBits_t uxBits = xEventGroupWaitBits(
    //     xWifiStatGroup,
    //     WIFI_NO_CONNECTING_BIT,
    //     pdFALSE,
    //     pdFALSE,   // 任一置位即返回
    //     portMAX_DELAY
    // );

    // uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    // wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    // uint16_t ap_count = 0;
    // memset(ap_info, 0, sizeof(ap_info));

#ifdef USE_CHANNEL_BITMAP
    wifi_scan_config_t *scan_config = (wifi_scan_config_t *)calloc(1,sizeof(wifi_scan_config_t));
    if (!scan_config) {
        ESP_LOGE(TAG, "Memory Allocation for scan config failed!");
        return;
    }
    array_2_channel_bitmap(channel_list, CHANNEL_LIST_SIZE, scan_config);
    esp_wifi_scan_start(scan_config, true);
    free(scan_config);

#else
    esp_wifi_scan_start(NULL, true);
#endif /*USE_CHANNEL_BITMAP*/

    // ESP_LOGI(TAG, "Max AP number ap_info can hold = %u", number);
    // ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(number, ap_info));
    // ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
    // for (int i = 0; i < number; i++) {
    //     ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
    //     ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
    //     print_auth_mode(ap_info[i].authmode);
    //     // if (ap_info[i].akm_dpp) {
    //     //     ESP_LOGI(TAG, "DPP \t\tSupported%s", (ap_info[i].authmode != WIFI_AUTH_DPP) ? " (mixed mode)" : " (DPP-only)");
    //     // }
    //     if (ap_info[i].authmode != WIFI_AUTH_WEP) {
    //         print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
    //     }
    // }
    // ESP_LOGI(TAG, "Channel \t\t%d", ap_info[i].primary);
}

// static int s_retry_num = 0;

static void show_statusbar_wifi(lv_timer_t *)
{
    if (ui_statusbar_wifi)
        lv_obj_remove_flag(ui_statusbar_wifi, LV_OBJ_FLAG_HIDDEN);
}

static void hide_statusbar_wifi(lv_timer_t *)
{
    if (ui_statusbar_wifi)
        lv_obj_add_flag(ui_statusbar_wifi, LV_OBJ_FLAG_HIDDEN);
}

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    ESP_LOGI(TAG, "event_base = %s, event_id = %d", event_base, event_id);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        // if (s_retry_num < 3) {
        //     esp_wifi_connect();
        //     s_retry_num++;
        //     ESP_LOGI(TAG, "retry to connect to the AP");
        // } else {
        //     s_retry_num = 0;
        //     xEventGroupSetBits(xWifiEventGroup, WIFI_FAIL_BIT);
        //     ESP_LOGI(TAG,"connect to the AP fail");
        // }
        
            // xEventGroupSetBits(xWifiEventGroup, WIFI_FAIL_BIT);

        wifi_event_sta_disconnected_t *ev = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGW(TAG, "disconnected, reason= %d",  ev->reason);

        switch (ev->reason)
        {
        case WIFI_REASON_ASSOC_LEAVE:
            // 调用esp_wifi_disconnect();
            break;
        
        default:
            xEventGroupSetBits(xWifiEventGroup, WIFI_FAIL_BIT);
            break;
        }

        isconnected = false;
        lv_timer_set_repeat_count(lv_timer_create(hide_statusbar_wifi, 0, NULL), 1);
        // ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        // s_retry_num = 0;

        xEventGroupSetBits(xWifiEventGroup, WIFI_CONNECTED_BIT);
            
        time_sync();
        isconnected = true;
        lv_timer_set_repeat_count(lv_timer_create(show_statusbar_wifi, 0, NULL), 1);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_SCAN_DONE) {
        
    }
}

void wifi_init_sta()
{
    // Initialize NVS
    // nvs_flash_erase();
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));


    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    xWifiEventGroup = xEventGroupCreate();
    xWifiStatGroup = xEventGroupCreate();
}

void wifi_connect(wifi_ap_record_t *ap_record, const char * password)
{
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = ap_record->authmode,
        },
    };
    
    strncpy((char *)wifi_config.sta.ssid, (char *) ap_record->ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    

    esp_wifi_disconnect();
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_connect();
}

static void wifi_item_event_handler(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    wifi_ap_record_t *ap_record = (wifi_ap_record_t *) lv_event_get_user_data(e);
    if (code == LV_EVENT_CLICKED) {
        void *data = lv_malloc(sizeof(wifi_ap_record_t));
        memcpy(data, ap_record, sizeof(wifi_ap_record_t));
        ESP_LOGI(TAG, "%s %p", __func__, data);

        lv_obj_set_user_data(ui_wifi_input_pwd, data);
        lv_textarea_set_text(ui_wifi_pwd_TextArea, "");
        _ui_screen_change(&ui_wifi_input_pwd, LV_SCR_LOAD_ANIM_OVER_LEFT, 300, 0, &ui_wifi_input_pwd_screen_init);
    }
    else if (code == LV_EVENT_DELETE) {
        if (ap_record)
        {
            free(ap_record);
        }
    }

}

static wifi_ap_record_t ap_info[16];
static uint16_t number = 16;

static void vWifiScanTask(void *pvParameters)
{
    bool scan = true;
    while (1)
    {
        if (scan)
        {
            memset(ap_info, 0, sizeof(ap_info));
            wifi_scan(ap_info, &number);
        }

        wifi_ap_record_t c_ap_info;
        
        // 获取当前连接的 AP 信息
        esp_err_t ret = esp_wifi_sta_get_ap_info(&c_ap_info);
        if (ret == ESP_OK) {
            // ESP_LOGI(TAG, "=== 当前 WiFi 信息 ===");
            // ESP_LOGI(TAG, "SSID: %s", c_ap_info.ssid);
            // ESP_LOGI(TAG, "RSSI: %d dBm", c_ap_info.rssi);
            // ESP_LOGI(TAG, "Channel: %d", c_ap_info.primary);
            // ESP_LOGI(TAG, "Authmode: %d", c_ap_info.authmode);
            // ESP_LOGI(TAG, "Pairwise Cipher: %d", c_ap_info.pairwise_cipher);
            // ESP_LOGI(TAG, "Group Cipher: %d", c_ap_info.group_cipher);
            
            // 打印 MAC 地址
            // ESP_LOGI(TAG, "BSSID: " MACSTR, MAC2STR(c_ap_info.bssid));
        } else {
            // ESP_LOGW(TAG, "获取 WiFi 信息失败: %s", esp_err_to_name(ret));

            esp_wifi_connect();
        }

        if (ui____initial_actions0)
        {
            ui_lock();
            lv_obj_clean(ui_wifi_scan_list);
            ui_unlock();

            for (int i = 0; i < number; i++) {
                wifi_ap_record_t *ap_copy = malloc(sizeof(wifi_ap_record_t));
                memcpy(ap_copy, &ap_info[i], sizeof(wifi_ap_record_t));

                // ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
                // ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
                // ESP_LOGI(TAG, "authmode \t\t%d", ap_info[i].authmode);

                ui_lock();

                lv_obj_t *ui_wifi_scan_item = lv_obj_create(ui_wifi_scan_list);
                lv_obj_remove_style_all(ui_wifi_scan_item);
                lv_obj_set_height(ui_wifi_scan_item, 43);
                lv_obj_set_width(ui_wifi_scan_item, lv_pct(100));
                lv_obj_remove_flag(ui_wifi_scan_item, LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_SCROLLABLE);      /// Flags
                lv_obj_set_style_text_font(ui_wifi_scan_item, &ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);

                ui_Container56 = lv_obj_create(ui_wifi_scan_item);
                lv_obj_set_name(ui_Container56, "wifi_item");
                lv_obj_remove_style_all(ui_Container56);
                lv_obj_set_height(ui_Container56, 40);
                lv_obj_set_width(ui_Container56, lv_pct(90));
                lv_obj_set_align(ui_Container56, LV_ALIGN_BOTTOM_MID);
                lv_obj_set_flex_flow(ui_Container56, LV_FLEX_FLOW_ROW);
                lv_obj_set_flex_align(ui_Container56, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
                lv_obj_remove_flag(ui_Container56, LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_PRESS_LOCK);      /// Flags
                lv_obj_set_style_radius(ui_Container56, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_color(ui_Container56, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
                lv_obj_set_style_bg_opa(ui_Container56, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

                ui_wifi_rssi = lv_image_create(ui_Container56);
                lv_image_set_src(ui_wifi_rssi, &ui_img_icon_wifi_black_png);
                lv_obj_set_height(ui_wifi_rssi, LV_SIZE_CONTENT);    /// 1
                lv_obj_set_flex_grow(ui_wifi_rssi, 1);
                lv_obj_set_align(ui_wifi_rssi, LV_ALIGN_CENTER);
                lv_obj_remove_flag(ui_wifi_rssi, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                                LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE);     /// Flags

                ui_wifi_ssid = lv_label_create(ui_Container56);
                lv_obj_set_height(ui_wifi_ssid, LV_SIZE_CONTENT);    /// 1
                lv_obj_set_flex_grow(ui_wifi_ssid, 3);
                lv_obj_set_align(ui_wifi_ssid, LV_ALIGN_CENTER);
                lv_label_set_text(ui_wifi_ssid, (const char *) ap_info[i].ssid);
                lv_obj_remove_flag(ui_wifi_ssid, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE);      /// Flags

                ui_Container57 = lv_obj_create(ui_Container56);
                lv_obj_remove_style_all(ui_Container57);
                lv_obj_set_height(ui_Container57, 40);
                lv_obj_set_flex_grow(ui_Container57, 1);
                lv_obj_set_align(ui_Container57, LV_ALIGN_CENTER);
                lv_obj_remove_flag(ui_Container57, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE |
                                LV_OBJ_FLAG_SCROLLABLE);     /// Flags

                ui_wifi_auth_mode = lv_image_create(ui_Container57);
                if (ret == ESP_OK)
                {
                    int ssid_len = strlen((char *) ap_info[i].ssid);
                    int len1 = strncmp((char *) c_ap_info.ssid, (char *) ap_info[i].ssid, ssid_len);
                    int len2 = memcmp(c_ap_info.bssid, ap_info[i].ssid, 6);
                    if (len1 == 0 || len2 == 0) {
                        lv_image_set_src(ui_wifi_auth_mode, &ui_img_icon_circle_check_png);
                        lv_obj_move_to_index(ui_wifi_scan_item, 0);
                    } 
                    else
                    {
                        if (ap_info[i].authmode != WIFI_AUTH_OPEN) {
                            lv_image_set_src(ui_wifi_auth_mode, &ui_img_icon_lock_png);
                        }
                    }
                }
                else
                {
                    if (ap_info[i].authmode != WIFI_AUTH_OPEN) {
                        lv_image_set_src(ui_wifi_auth_mode, &ui_img_icon_lock_png);
                    }
                }
                lv_obj_set_name(ui_wifi_auth_mode, "wifi_status");
                lv_obj_set_width(ui_wifi_auth_mode, LV_SIZE_CONTENT);   /// 1
                lv_obj_set_height(ui_wifi_auth_mode, LV_SIZE_CONTENT);    /// 1
                lv_obj_set_align(ui_wifi_auth_mode, LV_ALIGN_CENTER);
                lv_obj_remove_flag(ui_wifi_auth_mode,
                                LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_SNAPPABLE |
                                LV_OBJ_FLAG_SCROLLABLE);     /// Flags

                lv_obj_add_event_cb(ui_Container56, wifi_item_event_handler, LV_EVENT_ALL, ap_copy);
                ui_unlock();
            }
        }

        uint32_t ulNotified;
        if(xTaskNotifyWait(0xFFFFFFFF, 0xFFFFFFFF, &ulNotified, pdMS_TO_TICKS(60 * 1000)) == pdTRUE)
        {
            scan = false;
        }
        else
        {
            //定时扫描
            scan = true;
        }
    }
}

void wifi_scan_init()
{
    if (wifi_scan_task == NULL)
    {
        xTaskCreate(vWifiScanTask, "wifi scann", 8192, NULL, 5, &wifi_scan_task);
    }
}
