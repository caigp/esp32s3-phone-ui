// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <freertos/FreeRTOS.h>
#include <freertos/timers.h>
#include <freertos/task.h>
#include <freertos/queue.h>
//Nes stuff wants to define this as well...
#undef false
#undef true
#undef bool


#include <math.h>
#include <string.h>
#include <noftypes.h>
#include <bitmap.h>
#include <nofconfig.h>
#include <event.h>
#include <gui.h>
#include <log.h>
#include <nes.h>
#include <nes_pal.h>
#include <nesinput.h>
#include <osd.h>
#include <stdint.h>
// #include "driver/i2s.h"
#include "sdkconfig.h"
// #include <spi_lcd.h>

#include <psxcontroller.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <test.h>
#include "driver/gpio.h"

static const char *TAG = "VIDEO AUDIO";

#define  DEFAULT_SAMPLERATE   44100


#define  DEFAULT_WIDTH        256
#define  DEFAULT_HEIGHT       NES_VISIBLE_HEIGHT

#define  REFRESH_RATE         60
#define  AUDIO_FRAME_SAMPLES  (DEFAULT_SAMPLERATE / REFRESH_RATE)

// 标准 64 色 NES RGB565 调色板 (标准 RGB565 格式)
const uint16_t nes_palette_rgb565[64] = {
    0x738E, 0x20D6, 0x0015, 0x4013, 0x880E, 0xA802, 0xA000, 0x7840,
    0x4140, 0x0200, 0x0280, 0x01C2, 0x19CB, 0x0000, 0x0000, 0x0000,
    0xBDF7, 0x039D, 0x21DD, 0x801B, 0xB813, 0xE007, 0xD800, 0xB180,
    0x6B80, 0x0380, 0x0480, 0x03C3, 0x03CB, 0x0000, 0x0000, 0x0000,
    0xFFFF, 0x3DFE, 0x5CD1, 0x9A3F, 0xF27F, 0xF257, 0xF340, 0xF4A0,
    0xB5C0, 0x4DE0, 0x2E27, 0x1E32, 0x0619, 0x0000, 0x0000, 0x0000,
    0xFFFF, 0xAEFF, 0xC5FF, 0xDDBF, 0xFDDF, 0xFCD6, 0xFDCE, 0xFE27,
    0xE68E, 0xBEEF, 0x9F73, 0x9F7B, 0x073B, 0x0000, 0x0000, 0x0000
};

TimerHandle_t timer;

volatile short key_board = 0xff;
static bool sound = true;

//Seemingly, this will be called only once. Should call func with a freq of frequency,
int osd_installtimer(int frequency, void *func, int funcsize, void *counter, int countersize)
{
	printf("Timer install, freq=%d\n", frequency);
	timer=xTimerCreate("nes",pdMS_TO_TICKS(1000 / frequency), pdTRUE, NULL, func);
	xTimerStart(timer, 0);
   return 0;
}

void osd_uninstalltimer()
{
    if (timer !=NULL)
    {
        xTimerDelete(timer, 0);
        timer = NULL;
    }
}

/*
** Audio
*/
static void (*audio_callback)(void *buffer, int length) = NULL;
/* Audio is enabled by the runtime sound toggle in this board's UI. */
static uint16_t *audio_frame;

FILE *f;
void do_audio_frame() {
    if (!sound || audio_callback == NULL || audio_frame == NULL)
    {
        return;
    }

    /* Generate and submit one complete NES frame of audio.  Splitting the
     * same 735 samples into six ring-buffer writes adds avoidable mutex and
     * wakeup overhead and can make the emulation task wait repeatedly when
     * the mixer is busy. */
    audio_callback(audio_frame, AUDIO_FRAME_SAMPLES);
    nes_audio_callback(audio_frame, AUDIO_FRAME_SAMPLES * sizeof(*audio_frame));
}

void osd_setsound(void (*playfunc)(void *buffer, int length))
{
   //Indicates we should call playfunc() to get more data.
   audio_callback = playfunc;
}

static void osd_stopsound(void)
{
   audio_callback = NULL;
   nes_audio_deinit();
}


static int osd_init_sound(void)
{
	audio_frame=malloc(AUDIO_FRAME_SAMPLES * sizeof(*audio_frame));
	audio_callback = NULL;

    nes_audio_init();
	return 0;
}

void osd_getsoundinfo(sndinfo_t *info)
{
   info->sample_rate = DEFAULT_SAMPLERATE;
   info->bps = 16;
}

/*
** Video
*/

static int init(int width, int height);
static void shutdown(void);
static int set_mode(int width, int height, int pitch);
static void set_palette(rgb_t *pal);
static void clear(uint8 color);
static bitmap_t *lock_write(void);
static void free_write(int num_dirties, rect_t *dirty_rects);
static void custom_blit(bitmap_t *bmp, int num_dirties, rect_t *dirty_rects);


// 将调色板数组强制放入外部 RAM
uint16 myPalette[256]; 

viddriver_t sdlDriver =
{
   "Simple DirectMedia Layer",         /* name */
   init,          /* init */
   shutdown,      /* shutdown */
   set_mode,      /* set_mode */
   set_palette,   /* set_palette */
   clear,         /* clear */
   lock_write,    /* lock_write */
   free_write,    /* free_write */
   custom_blit,   /* custom_blit */
   false          /* invalidate flag */
};


bitmap_t *myBitmap;

/* LVGL reads the published buffer while the NES task fills the other one. */
static uint16_t *rgb565;

void osd_getvideoinfo(vidinfo_t *info)
{
   info->default_width = NES_SCREEN_WIDTH;
   info->default_height = NES_SCREEN_HEIGHT;
   info->driver = &sdlDriver;
}

/* flip between full screen and windowed */
void osd_togglefullscreen(int code)
{
}

/* initialise video */
static int init(int width, int height)
{
    rgb565 = (uint16_t *)heap_caps_malloc(width * height * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    memset(rgb565, 0, sizeof(uint16_t) * width * height);
	return 0;
}

static void shutdown(void)
{
    /* Wait for LVGL rendering and detach its source before freeing pixels. */
    nes_video_deinit();
    heap_caps_free(rgb565);
}

/* set a video mode */
static int set_mode(int width, int height,  int pitch)
{
    nes_img_set(width, height);
    return 0;
}

/* copy nes palette over to hardware */
static void set_palette(rgb_t *pal)
{
    
// 	uint16 c;
//    int i;

//    for (i = 0; i < 256; i++)
//    {
//     // ESP_LOGI(TAG, "%d %d %d\n", pal[i].r, pal[i].g, pal[i].b);
//       c=(pal[i].b>>3)+((pal[i].g>>2)<<5)+((pal[i].r>>3)<<11);
//         // c = (pal[i].r >> 3) + ((pal[i].g >> 2) << 5) + ((pal[i].b >> 3) << 11);
//       myPalette[i]=c;
//    }

}

void init_256_nes_palette(bool swap_bytes, bool is_bgr) {
    for (int i = 0; i < 256; i++) {
        // 使用 % 64 或 & 0x3F 自动循环映射 64 基础色
        uint16_t color = nes_palette_rgb565[i & 0x3F]; 
        
        // BGR 通道对调处理
        if (is_bgr) {
            uint16_t r = (color >> 11) & 0x1F;
            uint16_t g = (color >> 5) & 0x3F;
            uint16_t b = color & 0x1F;
            color = (b << 11) | (g << 5) | r;
        }

        // 字节序颠倒处理
        if (swap_bytes) {
            color = __builtin_bswap16(color);
        }

        myPalette[i] = color;
    }
}

/* clear all frames to a particular color */
static void clear(uint8 color)
{

}



/* acquire the directbuffer for writing */
static bitmap_t *lock_write(void)
{

}

/* release the resource */
static void free_write(int num_dirties, rect_t *dirty_rects)
{

}

static void custom_blit(bitmap_t *bmp, int num_dirties, rect_t *dirty_rects) {

    int w = bmp->width;
    int h = bmp->height;
    for (size_t i = 0; i < h; i++)
    {
        uint8_t *buf = bmp->line[i];

        for (size_t j = 0; j < w; j++) {
            rgb565[i * w + j] = myPalette[buf[j]];

            // rgb565[i * 272 + j] = myPalette[src[i * 272 + j]];

        }
    }

    nes_video_callback(rgb565);
}

/*
** Input
*/

static void osd_initinput()
{
	// psxcontrollerInit();
}

void osd_getinput(void)
{
// 	const int ev[16]={
// 			event_joypad1_select,0,0,event_joypad1_start,event_joypad1_up,event_joypad1_right,event_joypad1_down,event_joypad1_left,
// 			0,0,0,0,event_soft_reset,event_joypad1_a,event_joypad1_b,event_hard_reset
// 		};
// 	static int oldb=0xffff;
// 	int b=psxReadInput();
// 	int chg=b^oldb;
// 	int x;
// 	oldb=b;
// 	event_t evh;
// //	printf("Input: %x\n", b);
// 	for (x=0; x<16; x++) {
// 		if (chg&1) {
// 			evh=event_get(ev[x]);
// 			if (evh) evh((b&1)?INP_STATE_BREAK:INP_STATE_MAKE);
// 		}
// 		chg>>=1;
// 		b>>=1;
// 	}

    short s = key_board;
    const int ev[8]={
                event_joypad1_a,
                event_joypad1_b, 
                event_joypad1_start,
                event_joypad1_select,
                event_joypad1_up,
                event_joypad1_down,
                event_joypad1_left,
                event_joypad1_right,
            };

    event_t evh;
    for (size_t i = 0; i < 8; i++)
    {
        evh = event_get(ev[i]);
        if (evh)
        {
            evh((s & 1) ? INP_STATE_BREAK : INP_STATE_MAKE);
        }
        s >>= 1;
    }

}

bool nes_key_set(int num, int action)
{
    int v = action;
    // ESP_LOGI(TAG, "v = %d", v);
    if (v != 0 && v != 1)
    {
        return timer != NULL;
    }
    
    switch (num)
    {
    case GPIO_NUM_15:
        key_board = (key_board & ~(1 << 6)) | (v << 6);
        break;
    case GPIO_NUM_16:
        key_board = (key_board & ~(1 << 5)) | (v << 5);
        break;
    case GPIO_NUM_17:
        key_board = (key_board & ~(1 << 4)) | (v << 4);
        break;
    case GPIO_NUM_18:
        key_board = (key_board & ~(1 << 7)) | (v << 7);
        break;    
    case GPIO_NUM_8:
        key_board = (key_board & ~(1 << 1)) | (v << 1);
        break;
    case GPIO_NUM_9:
        key_board = (key_board & ~(1 << 0)) | (v << 0);
        break;
    case GPIO_NUM_3:
        key_board = (key_board & ~(1 << 3)) | (v << 3);
        break;
    case GPIO_NUM_46:
        key_board = (key_board & ~(1 << 2)) | (v << 2);
        break;                
    default:
        break;
    }

    // ESP_LOGI(TAG, "func = %s, key_board = %d", __func__, key_board);

    return timer != NULL;
}

static void osd_freeinput(void)
{
}

void osd_getmouse(int *x, int *y, int *button)
{
}

/*
** Shutdown
*/

/* this is at the bottom, to eliminate warnings */
void osd_shutdown()
{
    ESP_LOGI("NES", "%s", __func__);
    nes_video_deinit();
	osd_stopsound();
	osd_freeinput();
    osd_uninstalltimer();
}

static int logprint(const char *string)
{
    ESP_LOGI("NES", "%s", string);
    return 0;
}

/*
** Startup
*/

int osd_init()
{
	log_chain_logfunc(logprint);
    init_256_nes_palette(false, false);

	if (osd_init_sound())
		return -1;

    
	osd_initinput();
    nes_video_init();

	return 0;
}

void nes_toggle_pause()
{
    event_t evh = event_get(event_togglepause);
    if (evh)
    {
        evh(INP_STATE_MAKE);
    }
}

bool nes_toggle_sound()
{
    sound = !sound;
    return sound;
}
