#include "../../ui.h"
#include "esp_chip_info.h"
#include "esp_mac.h"
#include "esp_clk_tree.h"
#include "esp_heap_caps.h"
#include "esp_flash.h"

lv_obj_t * ui_device_info;

typedef struct {
    lv_obj_t *item;
    lv_obj_t *key;
    lv_obj_t *value;
} item_components_t;

static item_components_t sram_comp;
static item_components_t psram_comp;

const char* chip_model_to_str(esp_chip_model_t model) {
    switch (model) {
        case CHIP_ESP32:    return "ESP32";
        case CHIP_ESP32S2:  return "ESP32-S2";
        case CHIP_ESP32S3:  return "ESP32-S3";
        case CHIP_ESP32C3:  return "ESP32-C3";
        case CHIP_ESP32C2:  return "ESP32-C2";
        case CHIP_ESP32C6:  return "ESP32-C6";
        case CHIP_ESP32H2:  return "ESP32-H2";
        case CHIP_ESP32P4:  return "ESP32-P4";
        case CHIP_ESP32C61: return "ESP32-C61";
        case CHIP_ESP32C5:  return "ESP32-C5";
        case CHIP_ESP32H21: return "ESP32-H21";
        case CHIP_ESP32H4:  return "ESP32-H4";
        case CHIP_POSIX_LINUX: return "POSIX/Linux";
        default:            return "Unknown";
    }
}

static item_components_t create_item(lv_obj_t * parent, const void *k, const char * fmt, ...)
{
    item_components_t comp = {0};

    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_remove_style_all(item);
    lv_obj_set_height(item, 30);
    lv_obj_set_width(item, lv_pct(100));
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_flag(item, LV_OBJ_FLAG_PRESS_LOCK | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
                       LV_OBJ_FLAG_SNAPPABLE | LV_OBJ_FLAG_SCROLLABLE);     /// Flags
    lv_obj_set_style_radius(item, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(item, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(item, ui_font_simhei14, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(item, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(item, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * key = lv_label_create(item);
    lv_obj_set_flex_grow(key, 1);
    lv_obj_set_style_text_align(key, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(key, k);

    lv_obj_t * value = lv_label_create(item);
    lv_obj_set_flex_grow(value, 1);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);

    char buf[64];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    lv_label_set_text(value, buf);

    comp.item = item;
    comp.key = key;
    comp.value = value;
    return comp;
}

static void ui_event_device_info(lv_event_t * e)
{ 
    lv_event_code_t event_code = lv_event_get_code(e);

    if(event_code == LV_EVENT_SCREEN_LOADED) {
        uint32_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

        char buf[32];
        snprintf(buf, sizeof(buf), "%.2f KB", (internal_free / 1024.0));
        lv_label_set_text(sram_comp.value, buf);

        uint32_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
        uint32_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        
        snprintf(buf, sizeof(buf), "%.2f MB/%.2f MB", (psram_free / (1024.0 * 1024.0)), (psram_total / (1024.0 * 1024.0)));
        lv_label_set_text(psram_comp.value, buf);
    }
}

static void ui_event_back_btn(lv_event_t * e)
{
    _ui_screen_change(&ui_Settings, LV_SCR_LOAD_ANIM_OUT_RIGHT, 300, 0, &ui_Settings_screen_init);
}

// build funtions

void ui_device_info_init(void)
{
    ui_device_info = ui_base_create(NULL);

    lv_obj_t * display = ui_comp_get_child(ui_device_info, UI_COMP_DISPLAY_CONTAINER);
    ui_object_set_themeable_style_property(display, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_BG_COLOR,
                                           _ui_theme_color_white);
    ui_object_set_themeable_style_property(display, LV_PART_MAIN | LV_STATE_DEFAULT, LV_STYLE_BG_OPA,
                                           _ui_theme_alpha_white);
    lv_obj_set_style_pad_left(display, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(display, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(display, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(display, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_row(display, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_column(display, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_flag(display, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(display, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t * head_obj = lv_obj_create(display);
    lv_obj_remove_style_all(head_obj);
    lv_obj_set_height(head_obj, lv_pct(25));
    lv_obj_set_width(head_obj, lv_pct(100));
    lv_obj_remove_flag(head_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(head_obj, lv_color_hex(0x4682B4), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(head_obj, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(head_obj, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t * img = lv_img_create(head_obj);
    lv_img_set_src(img, &ui_img_liteui);
    lv_obj_set_align(img, LV_ALIGN_CENTER);
    

    lv_obj_t * back = lv_label_create(head_obj);
    lv_label_set_text(back, LV_SYMBOL_LEFT);
    lv_obj_align(back, LV_IMAGE_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_set_style_text_color(back, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_ext_click_area(back, 20);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);

    esp_chip_info_t info;
    esp_chip_info(&info);
    create_item(display, "Model", "%s", chip_model_to_str(info.model));
    create_item(display, "Cpu Cores", "%d", info.cores);

    uint32_t cpu_freq_hz = 0;
    esp_clk_tree_src_get_freq_hz(SOC_MOD_CLK_CPU, ESP_CLK_TREE_SRC_FREQ_PRECISION_EXACT, &cpu_freq_hz);
    create_item(display, "CPU 主频", "%lu MHz", cpu_freq_hz / 1000000);

    uint8_t mac[6];
    esp_base_mac_addr_get(mac);
    create_item(display, "MAC", MACSTR, MAC2STR(mac));

    uint32_t flash_size = 0;
    esp_flash_get_size(NULL, &flash_size);
    create_item(display, "Flash", "%.2f MB", flash_size / (1024.0 * 1024.0));

    uint32_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    sram_comp = create_item(display, "SRAM 可用", "%.2f KB", internal_free / 1024.0);

    uint32_t psram_total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    uint32_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    psram_comp = create_item(display, "PSRAM", "%.2f MB/%.2f MB", psram_free / (1024.0 * 1024.0), psram_total / (1024.0 * 1024.0));

    lv_obj_add_event_cb(ui_device_info, ui_event_device_info, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(back, ui_event_back_btn, LV_EVENT_CLICKED, NULL);
}

void ui_device_info_destroy(void)
{
    if (ui_device_info)
    {
        lv_obj_del(ui_device_info);
        ui_device_info = NULL;
    }
}
