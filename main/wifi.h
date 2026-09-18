#pragma once

#include "esp_wifi.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define WIFI_NO_CONNECTING_BIT BIT2

extern EventGroupHandle_t xWifiEventGroup;
extern EventGroupHandle_t xWifiStatGroup;

extern bool wifi_isconnected;

extern void wifi_scan(wifi_ap_record_t *ap_info, uint16_t *number);
extern void wifi_init_sta();
extern void wifi_connect(wifi_ap_record_t *ap_record, const char *password);

extern void wifi_scan_init();