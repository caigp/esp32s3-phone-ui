#ifndef TIME_FORMAT_H
#define TIME_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 格式选项
typedef enum {
    TIME_FMT_HH_MM,
    TIME_FMT_MM_SS,       // 00:00
    TIME_FMT_HH_MM_SS,    // 00:00:00
    TIME_FMT_MM_SS_MS,    // 00:00.000
    TIME_FMT_HH_MM_SS_MS  // 00:00:00.000
} TimeFormat;

// 工具函数（返回字符串长度，0表示失败）
int format_seconds(uint32_t seconds, char *buffer, size_t size, TimeFormat fmt);
int format_millis(uint64_t millis, char *buffer, size_t size, TimeFormat fmt);

// 解析函数
bool parse_time_seconds(const char *str, uint32_t *out_seconds);
bool parse_time_millis(const char *str, uint64_t *out_millis);

// 内部辅助函数的静态声明（用于宏实现）
static inline char* format_seconds_to_str(uint32_t seconds, TimeFormat fmt) {
    static char buffer[64];  // 静态缓冲区，避免栈溢出
    format_seconds(seconds, buffer, sizeof(buffer), fmt);
    return buffer;
}

static inline char* format_millis_to_str(uint64_t millis, TimeFormat fmt) {
    static char buffer[64];
    format_millis(millis, buffer, sizeof(buffer), fmt);
    return buffer;
}

// 便捷宏 - 返回 int（字符串长度）
#define FORMAT_SECONDS_LEN(sec)     format_seconds(sec, (char[32]){0}, 32, TIME_FMT_MM_SS)
#define FORMAT_SECONDS_FULL_LEN(sec) format_seconds(sec, (char[32]){0}, 32, TIME_FMT_HH_MM_SS)
#define FORMAT_MILLIS_LEN(ms)       format_millis(ms, (char[32]){0}, 32, TIME_FMT_MM_SS_MS)
#define FORMAT_MILLIS_FULL_LEN(ms)  format_millis(ms, (char[32]){0}, 32, TIME_FMT_HH_MM_SS_MS)

// 便捷宏 - 返回 char* 字符串指针（注意：内部使用复合字面量，作用域内有效）
#define FORMAT_SECONDS(sec)         format_seconds_to_str(sec, TIME_FMT_MM_SS)
#define FORMAT_SECONDS_FULL(sec)    format_seconds_to_str(sec, TIME_FMT_HH_MM_SS)
#define FORMAT_MILLIS(ms)           format_millis_to_str(ms, TIME_FMT_MM_SS_MS)
#define FORMAT_MILLIS_FULL(ms)      format_millis_to_str(ms, TIME_FMT_HH_MM_SS_MS)

#ifdef __cplusplus
}
#endif

#endif // TIME_FORMAT_H