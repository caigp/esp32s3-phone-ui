#ifndef FILE_UTILS_H
#define FILE_UTILS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 文件信息结构体
typedef struct {
    char name[128];        // 文件名（包含扩展名）
    char path[256];        // 完整路径
    uint32_t size;         // 文件大小（字节）
    uint32_t modified;     // 修改时间（Unix时间戳）
    uint8_t is_dir;        // 是否为目录 0:文件 1:目录
} file_info_t;

// 过滤规则结构体
typedef struct {
    char ext[32];          // 文件后缀，如"txt"、"jpg"
    uint8_t enabled;       // 是否启用此过滤 1:启用 0:禁用
} ext_filter_t;

// 最大过滤规则数量
#define MAX_EXT_FILTERS 16

// 文件列表句柄（不透明指针，保护内部实现）
typedef void* file_list_handle_t;

/**
 * @brief 创建文件列表（支持多后缀过滤）
 * 
 * @param folder_path 文件夹路径，如"/spiffs"或"/sdcard"
 * @param recursive 是否递归子目录 1:递归 0:不递归
 * @param filters 过滤规则数组，为NULL表示不过滤
 * @param filter_count 过滤规则数量
 * @param case_sensitive 后缀匹配是否区分大小写
 * @return file_list_handle_t 文件列表句柄，失败返回NULL
 */
file_list_handle_t file_list_create_ex(const char* folder_path, 
                                      uint8_t recursive,
                                      const ext_filter_t* filters,
                                      uint32_t filter_count,
                                      uint8_t case_sensitive);

/**
 * @brief 创建文件列表（兼容旧版接口，单后缀过滤）
 * 
 * @param folder_path 文件夹路径
 * @param recursive 是否递归子目录
 * @param filter_ext 单个文件后缀，NULL表示不过滤
 * @param case_sensitive 是否区分大小写
 * @return file_list_handle_t 文件列表句柄
 */
file_list_handle_t file_list_create(const char* folder_path, 
                                   uint8_t recursive,
                                   const char* filter_ext,
                                   uint8_t case_sensitive);

/**
 * @brief 重新扫描文件夹，更新文件列表
 * 
 * @param handle 文件列表句柄
 * @param recursive 是否递归子目录
 * @return 0成功，-1失败
 */
int32_t file_list_rescan(file_list_handle_t handle, uint8_t recursive);

/**
 * @brief 重新扫描并更改过滤规则
 * 
 * @param handle 文件列表句柄
 * @param recursive 是否递归子目录
 * @param filters 新的过滤规则数组
 * @param filter_count 过滤规则数量
 * @param case_sensitive 是否区分大小写
 * @return 0成功，-1失败
 */
int32_t file_list_rescan_with_filters(file_list_handle_t handle,
                                     uint8_t recursive,
                                     const ext_filter_t* filters,
                                     uint32_t filter_count,
                                     uint8_t case_sensitive);

/**
 * @brief 获取扫描统计信息
 * 
 * @param handle 文件列表句柄
 * @param total_scanned 输出总共扫描的文件数（包括被过滤的）
 * @param total_size 输出总大小（字节）
 * @param dir_count 输出目录数量
 * @param last_scan_time 输出上次扫描时间（Unix时间戳）
 * @return 0成功，-1失败
 */
int32_t file_list_get_stats(file_list_handle_t handle,
                           uint32_t* total_scanned,
                           uint64_t* total_size,
                           uint32_t* dir_count,
                           uint32_t* last_scan_time);

/**
 * @brief 获取文件总数
 * @param handle 文件列表句柄
 * @return 文件数量，失败返回-1
 */
int32_t file_list_get_count(file_list_handle_t handle);

/**
 * @brief 通过索引获取文件信息
 * 
 * @param handle 文件列表句柄
 * @param index 索引（从0开始）
 * @param info 输出的文件信息结构体指针
 * @return 0成功，-1失败
 */
int32_t file_list_get_by_index(file_list_handle_t handle, uint32_t index, file_info_t* info);

/**
 * @brief 通过文件名获取文件信息
 * 
 * @param handle 文件列表句柄
 * @param filename 文件名
 * @param info 输出的文件信息结构体指针
 * @return 0成功，-1失败
 */
int32_t file_list_get_by_name(file_list_handle_t handle, const char* filename, file_info_t* info);

/**
 * @brief 按后缀查找文件
 * 
 * @param handle 文件列表句柄
 * @param ext 要查找的文件后缀
 * @param info 输出的文件信息结构体指针
 * @param start_index 从该索引开始搜索
 * @return 找到的文件索引，-1未找到
 */
int32_t file_list_find_by_ext(file_list_handle_t handle, 
                             const char* ext, 
                             file_info_t* info,
                             uint32_t start_index);

/**
 * @brief 打印所有文件信息（调试用）
 * @param handle 文件列表句柄
 */
void file_list_print_all(file_list_handle_t handle);

/**
 * @brief 释放文件列表资源
 * @param handle 文件列表句柄
 */
void file_list_destroy(file_list_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // FILE_UTILS_H