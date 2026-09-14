#ifndef TEST_H
#define TEST_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 声明 LVGL 更新回调
void nes_video_callback(void *data);
void nes_audio_callback(const void *src, size_t size);

void nes_audio_init();
void nes_audio_deinit();
void nes_video_init();
void nes_video_deinit();
void nes_img_set(int pitch, int height);
bool nes_key_set(int num, int action);
void nes_toggle_pause();
bool nes_toggle_sound();

#ifdef __cplusplus
}
#endif

#endif /* TEST_H */