#include "time_format.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 格式化秒
int format_seconds(uint32_t seconds, char *buffer, size_t size, TimeFormat fmt) {
    if (!buffer || size < 2) return 0;

    uint32_t hours   = seconds / 3600;
    uint32_t minutes = (seconds % 3600) / 60;
    uint32_t secs    = seconds % 60;

    int len = 0;
    switch (fmt) {
        case TIME_FMT_MM_SS:
            len = snprintf(buffer, size, "%02u:%02u", 
                          (unsigned int)minutes, (unsigned int)secs);
            break;
        case TIME_FMT_HH_MM_SS:
            len = snprintf(buffer, size, "%02u:%02u:%02u", 
                          (unsigned int)hours, (unsigned int)minutes, (unsigned int)secs);
            break;
        case TIME_FMT_MM_SS_MS:
            // 秒格式时不包含毫秒，统一格式
            len = snprintf(buffer, size, "%02u:%02u.000", 
                          (unsigned int)minutes, (unsigned int)secs);
            break;
        case TIME_FMT_HH_MM_SS_MS:
            len = snprintf(buffer, size, "%02u:%02u:%02u.000", 
                          (unsigned int)hours, (unsigned int)minutes, (unsigned int)secs);
            break;
        default:
            return 0;
    }
    return len >= 0 ? len : 0;
}

// 格式化毫秒
int format_millis(uint64_t millis, char *buffer, size_t size, TimeFormat fmt) {
    if (!buffer || size < 2) return 0;

    uint32_t total_seconds = (uint32_t)(millis / 1000);
    uint32_t ms_part       = (uint32_t)(millis % 1000);

    uint32_t hours   = total_seconds / 3600;
    uint32_t minutes = (total_seconds % 3600) / 60;
    uint32_t secs    = total_seconds % 60;

    int len = 0;
    switch (fmt) {
        case TIME_FMT_MM_SS:
            len = snprintf(buffer, size, "%02u:%02u", 
                          (unsigned int)minutes, (unsigned int)secs);
            break;
        case TIME_FMT_HH_MM_SS:
            len = snprintf(buffer, size, "%02u:%02u:%02u", 
                          (unsigned int)hours, (unsigned int)minutes, (unsigned int)secs);
            break;
        case TIME_FMT_MM_SS_MS:
            len = snprintf(buffer, size, "%02u:%02u.%03u", 
                          (unsigned int)minutes, (unsigned int)secs, (unsigned int)ms_part);
            break;
        case TIME_FMT_HH_MM_SS_MS:
            len = snprintf(buffer, size, "%02u:%02u:%02u.%03u", 
                          (unsigned int)hours, (unsigned int)minutes, 
                          (unsigned int)secs, (unsigned int)ms_part);
            break;
        case TIME_FMT_HH_MM:
                len = snprintf(buffer, size, "%02u:%02u", 
                          (unsigned int)hours, (unsigned int)minutes);
            break;

        default:
            return 0;
    }
    return len >= 0 ? len : 0;
}

// 解析 "mm:ss" 或 "hh:mm:ss" 为秒
bool parse_time_seconds(const char *str, uint32_t *out_seconds) {
    if (!str || !out_seconds) return false;
    
    unsigned int h = 0, m = 0, s = 0;
    int consumed = 0;
    
    // 尝试 hh:mm:ss
    if (sscanf(str, "%u:%u:%u%n", &h, &m, &s, &consumed) == 3 && 
        str[consumed] == '\0') {
        *out_seconds = h * 3600 + m * 60 + s;
        return true;
    }
    
    // 尝试 mm:ss
    if (sscanf(str, "%u:%u%n", &m, &s, &consumed) == 2 && 
        str[consumed] == '\0') {
        *out_seconds = m * 60 + s;
        return true;
    }
    
    return false;
}

// 解析 "mm:ss.ms" 或 "hh:mm:ss.ms" 为毫秒
bool parse_time_millis(const char *str, uint64_t *out_millis) {
    if (!str || !out_millis) return false;
    
    unsigned int h = 0, m = 0, s = 0, ms = 0;
    int consumed = 0;
    
    // 尝试 hh:mm:ss.ms
    if (sscanf(str, "%u:%u:%u.%u%n", &h, &m, &s, &ms, &consumed) == 4 && 
        str[consumed] == '\0') {
        *out_millis = ((uint64_t)h * 3600 + m * 60 + s) * 1000 + ms;
        return true;
    }
    
    // 尝试 mm:ss.ms
    if (sscanf(str, "%u:%u.%u%n", &m, &s, &ms, &consumed) == 3 && 
        str[consumed] == '\0') {
        *out_millis = ((uint64_t)m * 60 + s) * 1000 + ms;
        return true;
    }
    
    return false;
}