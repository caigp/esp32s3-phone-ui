#include "file_utils.h"
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cctype>
#include "esp_log.h"

static const char* TAG = "FILE_UTILS";

class FileListImpl {
public:
    struct Entry {
        std::string name;
        std::string path;
        uint32_t size;
        uint32_t modified;
        bool is_dir;
        std::string extension;
    };
    
    struct Filter {
        std::string ext;
        bool enabled;
    };
    
    struct Stats {
        uint32_t total_scanned;      // 总共扫描的文件数（包括被过滤的）
        uint64_t total_size;         // 总大小（字节）
        uint32_t dir_count;          // 目录数量
        uint32_t last_scan_time;     // 上次扫描时间
    };
    
    std::vector<Entry> entries;
    std::string base_path;
    std::vector<Filter> filters;
    bool case_sensitive;
    bool recursive;                  // 保存递归设置
    Stats stats;
    
    FileListImpl() : case_sensitive(false), recursive(false) {
        memset(&stats, 0, sizeof(stats));
    }
    
    // 从文件名提取后缀
    static std::string getExtension(const std::string& filename) {
        size_t pos = filename.find_last_of('.');
        if (pos == std::string::npos || pos == filename.length() - 1) {
            return "";
        }
        return filename.substr(pos + 1);
    }
    
    // 字符串比较（根据大小写敏感设置）
    bool stringCompare(const std::string& str1, const std::string& str2) {
        if (case_sensitive) {
            return str1 == str2;
        }
        
        if (str1.length() != str2.length()) {
            return false;
        }
        
        for (size_t i = 0; i < str1.length(); i++) {
            if (std::tolower(str1[i]) != std::tolower(str2[i])) {
                return false;
            }
        }
        return true;
    }
    
    // 规范化后缀（移除前导点）
    static std::string normalizeExt(const std::string& ext) {
        std::string result = ext;
        if (!result.empty() && result[0] == '.') {
            result = result.substr(1);
        }
        return result;
    }
    
    // 检查是否匹配任意过滤条件
    bool matchesAnyFilter(const std::string& file_ext) {
        if (filters.empty()) {
            return true;  // 没有过滤条件，全部匹配
        }
        
        for (const auto& filter : filters) {
            if (filter.enabled && stringCompare(file_ext, filter.ext)) {
                return true;
            }
        }
        return false;
    }
    
    // 检查后缀是否已存在
    bool filterExists(const std::string& ext) {
        for (const auto& filter : filters) {
            if (stringCompare(filter.ext, ext)) {
                return true;
            }
        }
        return false;
    }
    
    // 移除过滤规则
    bool removeFilter(const std::string& ext) {
        for (auto it = filters.begin(); it != filters.end(); ++it) {
            if (stringCompare(it->ext, ext)) {
                filters.erase(it);
                return true;
            }
        }
        return false;
    }
    
    // 排序函数：目录优先，然后按文件名排序
    static bool compareEntries(const Entry& a, const Entry& b) {
        if (a.is_dir != b.is_dir) {
            return a.is_dir > b.is_dir;
        }
        return a.name < b.name;
    }
    
    // 递归遍历目录
    bool traverseDirectory(const std::string& path, bool recursive) {
        DIR* dir = opendir(path.c_str());
        if (dir == NULL) {
            ESP_LOGE(TAG, "Failed to open directory: %s", path.c_str());
            return false;
        }
        
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL) {
            // 跳过隐藏文件和特殊目录
            if (entry->d_name[0] == '.') continue;
            if (strcmp(entry->d_name, "System Volume Information") == 0) continue;
            
            std::string full_path = path;
            if (full_path.back() != '/') {
                full_path += "/";
            }
            full_path += entry->d_name;
            
            struct stat st;
            if (stat(full_path.c_str(), &st) != 0) {
                ESP_LOGW(TAG, "Failed to stat: %s", full_path.c_str());
                continue;
            }
            
            Entry new_entry;
            new_entry.name = entry->d_name;
            new_entry.path = full_path;
            new_entry.size = (uint32_t)st.st_size;
            new_entry.modified = (uint32_t)st.st_mtime;
            new_entry.is_dir = S_ISDIR(st.st_mode);
            new_entry.extension = getExtension(new_entry.name);
            
            if (new_entry.is_dir) {
                stats.dir_count++;
            } else {
                stats.total_scanned++;
                stats.total_size += new_entry.size;
                
                // 收集文件并根据过滤条件筛选
                if (matchesAnyFilter(new_entry.extension)) {
                    entries.push_back(new_entry);
                    ESP_LOGD(TAG, "Found file: %s (%u bytes)", 
                             new_entry.path.c_str(), new_entry.size);
                } else {
                    ESP_LOGD(TAG, "Filtered out: %s (ext=%s)", 
                             new_entry.path.c_str(), new_entry.extension.c_str());
                }
            }
            
            // 递归遍历子目录
            if (recursive && new_entry.is_dir) {
                traverseDirectory(full_path, recursive);
            }
        }
        
        closedir(dir);
        return true;
    }
    
    // 执行完整扫描
    bool performScan(bool recursive) {
        // 清空旧数据
        entries.clear();
        memset(&stats, 0, sizeof(stats));
        
        // 检查路径是否存在且是目录
        struct stat st;
        if (stat(base_path.c_str(), &st) != 0) {
            ESP_LOGE(TAG, "Path does not exist: %s", base_path.c_str());
            return false;
        }
        
        if (!S_ISDIR(st.st_mode)) {
            ESP_LOGE(TAG, "Path is not a directory: %s", base_path.c_str());
            return false;
        }
        
        // 遍历目录
        if (!traverseDirectory(base_path, recursive)) {
            ESP_LOGE(TAG, "Failed to traverse directory");
            return false;
        }
        
        // 排序
        std::sort(entries.begin(), entries.end(), compareEntries);
        
        // 更新扫描时间
        stats.last_scan_time = (uint32_t)time(NULL);
        
        ESP_LOGI(TAG, "Scan complete: %d files (scanned %d total, %d dirs)", 
                 (int)entries.size(), stats.total_scanned, stats.dir_count);
        
        return true;
    }
    
    // 重新过滤当前列表
    void refilterEntries() {
        std::vector<Entry> new_entries;
        for (auto& entry : entries) {
            if (matchesAnyFilter(entry.extension)) {
                new_entries.push_back(entry);
            }
        }
        entries = new_entries;
    }
    
    // 获取文件大小（单位格式化）
    static std::string formatSize(uint64_t size) {
        char buf[32];
        if (size < 1024) {
            snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)size);
        } else if (size < 1024 * 1024) {
            snprintf(buf, sizeof(buf), "%.1f KB", (double)size / 1024);
        } else if (size < 1024 * 1024 * 1024) {
            snprintf(buf, sizeof(buf), "%.1f MB", (double)size / (1024 * 1024));
        } else {
            snprintf(buf, sizeof(buf), "%.1f GB", (double)size / (1024 * 1024 * 1024));
        }
        return std::string(buf);
    }
    
    // 打印当前过滤规则
    void printFilters() {
        printf("Filters: %s [", case_sensitive ? "case-sensitive" : "case-insensitive");
        for (size_t i = 0; i < filters.size(); i++) {
            if (filters[i].enabled) {
                if (i > 0) printf(", ");
                printf("%s", filters[i].ext.c_str());
            }
        }
        printf("]\n");
    }
    
    // 打印统计信息
    void printStats() {
        char time_buf[64];
        struct tm* timeinfo = localtime((time_t*)&stats.last_scan_time);
        if (timeinfo) {
            strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", timeinfo);
        } else {
            snprintf(time_buf, sizeof(time_buf), "N/A");
        }
        
        printf("=== Scan Stats ===\n");
        printf("Total files scanned: %" PRIu32 "\n", stats.total_scanned);
        printf("Filtered files: %" PRIu32 "\n", 
            stats.total_scanned - (uint32_t)entries.size());
        printf("Total size: %s\n", formatSize(stats.total_size).c_str());
        printf("Directories: %" PRIu32 "\n", stats.dir_count);
        printf("Last scan: %s\n", time_buf);
        printf("==================\n");
    }
};

// C接口实现
extern "C" {

file_list_handle_t file_list_create_ex(const char* folder_path, 
                                      uint8_t recursive,
                                      const ext_filter_t* filters,
                                      uint32_t filter_count,
                                      uint8_t case_sensitive) {
    if (folder_path == NULL) {
        ESP_LOGE(TAG, "Invalid folder path");
        return NULL;
    }
    
    FileListImpl* impl = new (std::nothrow) FileListImpl();
    if (impl == NULL) {
        ESP_LOGE(TAG, "Memory allocation failed");
        return NULL;
    }
    
    impl->base_path = folder_path;
    impl->case_sensitive = case_sensitive != 0;
    impl->recursive = recursive != 0;
    
    // 设置多后缀过滤
    if (filters != NULL && filter_count > 0) {
        for (uint32_t i = 0; i < filter_count && i < MAX_EXT_FILTERS; i++) {
            if (filters[i].enabled && filters[i].ext[0] != '\0') {
                FileListImpl::Filter filter;
                filter.ext = FileListImpl::normalizeExt(filters[i].ext);
                filter.enabled = true;
                
                if (!impl->filterExists(filter.ext)) {
                    impl->filters.push_back(filter);
                    ESP_LOGI(TAG, "Added filter: %s", filter.ext.c_str());
                }
            }
        }
    }
    
    // 执行首次扫描
    if (!impl->performScan(recursive != 0)) {
        ESP_LOGE(TAG, "Initial scan failed");
        delete impl;
        return NULL;
    }
    
    ESP_LOGI(TAG, "File list created: %d files in %s", 
             (int)impl->entries.size(), folder_path);
    
    return (file_list_handle_t)impl;
}

file_list_handle_t file_list_create(const char* folder_path, 
                                   uint8_t recursive,
                                   const char* filter_ext,
                                   uint8_t case_sensitive) {
    ext_filter_t filter;
    
    if (filter_ext != NULL && filter_ext[0] != '\0') {
        memset(&filter, 0, sizeof(filter));
        strncpy(filter.ext, filter_ext, sizeof(filter.ext) - 1);
        filter.enabled = 1;
        return file_list_create_ex(folder_path, recursive, &filter, 1, case_sensitive);
    } else {
        return file_list_create_ex(folder_path, recursive, NULL, 0, case_sensitive);
    }
}

int32_t file_list_rescan(file_list_handle_t handle, uint8_t recursive) {
    if (handle == NULL) return -1;
    
    FileListImpl* impl = (FileListImpl*)handle;
    
    // 更新递归设置
    impl->recursive = recursive != 0;
    
    // 执行扫描
    if (!impl->performScan(recursive != 0)) {
        ESP_LOGE(TAG, "Rescan failed");
        return -1;
    }
    
    ESP_LOGI(TAG, "Rescan completed: %d files", (int)impl->entries.size());
    
    return 0;
}

int32_t file_list_rescan_with_filters(file_list_handle_t handle,
                                     uint8_t recursive,
                                     const ext_filter_t* filters,
                                     uint32_t filter_count,
                                     uint8_t case_sensitive) {
    if (handle == NULL) return -1;
    
    FileListImpl* impl = (FileListImpl*)handle;
    
    // 更新设置
    impl->recursive = recursive != 0;
    impl->case_sensitive = case_sensitive != 0;
    
    // 清除现有过滤
    impl->filters.clear();
    
    // 添加新过滤
    if (filters != NULL && filter_count > 0) {
        for (uint32_t i = 0; i < filter_count && i < MAX_EXT_FILTERS; i++) {
            if (filters[i].enabled && filters[i].ext[0] != '\0') {
                FileListImpl::Filter filter;
                filter.ext = FileListImpl::normalizeExt(filters[i].ext);
                filter.enabled = true;
                
                if (!impl->filterExists(filter.ext)) {
                    impl->filters.push_back(filter);
                }
            }
        }
    }
    
    // 执行扫描
    if (!impl->performScan(recursive != 0)) {
        ESP_LOGE(TAG, "Rescan with filters failed");
        return -1;
    }
    
    ESP_LOGI(TAG, "Rescan with new filters completed: %d files", 
             (int)impl->entries.size());
    
    return 0;
}

int32_t file_list_get_stats(file_list_handle_t handle,
                           uint32_t* total_scanned,
                           uint64_t* total_size,
                           uint32_t* dir_count,
                           uint32_t* last_scan_time) {
    if (handle == NULL) return -1;
    
    FileListImpl* impl = (FileListImpl*)handle;
    
    if (total_scanned) *total_scanned = impl->stats.total_scanned;
    if (total_size) *total_size = impl->stats.total_size;
    if (dir_count) *dir_count = impl->stats.dir_count;
    if (last_scan_time) *last_scan_time = impl->stats.last_scan_time;
    
    return 0;
}

int32_t file_list_set_filters(file_list_handle_t handle,
                             const ext_filter_t* filters,
                             uint32_t filter_count,
                             uint8_t case_sensitive) {
    if (handle == NULL) return -1;
    
    FileListImpl* impl = (FileListImpl*)handle;
    
    impl->filters.clear();
    impl->case_sensitive = case_sensitive != 0;
    
    if (filters != NULL && filter_count > 0) {
        for (uint32_t i = 0; i < filter_count && i < MAX_EXT_FILTERS; i++) {
            if (filters[i].enabled && filters[i].ext[0] != '\0') {
                FileListImpl::Filter filter;
                filter.ext = FileListImpl::normalizeExt(filters[i].ext);
                filter.enabled = true;
                
                if (!impl->filterExists(filter.ext)) {
                    impl->filters.push_back(filter);
                }
            }
        }
    }
    
    // 重新过滤
    impl->refilterEntries();
    ESP_LOGI(TAG, "Filters updated: %d extensions, %d files", 
             (int)impl->filters.size(), (int)impl->entries.size());
    
    return 0;
}

int32_t file_list_add_filter(file_list_handle_t handle, const char* ext) {
    if (handle == NULL || ext == NULL || ext[0] == '\0') return -1;
    
    FileListImpl* impl = (FileListImpl*)handle;
    
    std::string normalized = FileListImpl::normalizeExt(ext);
    
    if (impl->filterExists(normalized)) {
        ESP_LOGW(TAG, "Filter %s already exists", normalized.c_str());
        return 0;
    }
    
    if (impl->filters.size() >= MAX_EXT_FILTERS) {
        ESP_LOGE(TAG, "Maximum filters (%d) reached", MAX_EXT_FILTERS);
        return -1;
    }
    
    FileListImpl::Filter filter;
    filter.ext = normalized;
    filter.enabled = true;
    impl->filters.push_back(filter);
    
    // 重新过滤以包含新添加的类型
    impl->refilterEntries();
    
    ESP_LOGI(TAG, "Added filter: %s, total: %d files", 
             normalized.c_str(), (int)impl->entries.size());
    
    return 0;
}

int32_t file_list_remove_filter(file_list_handle_t handle, const char* ext) {
    if (handle == NULL || ext == NULL || ext[0] == '\0') return -1;
    
    FileListImpl* impl = (FileListImpl*)handle;
    
    std::string normalized = FileListImpl::normalizeExt(ext);
    
    if (impl->removeFilter(normalized)) {
        impl->refilterEntries();
        ESP_LOGI(TAG, "Removed filter: %s, total: %d files", 
                 normalized.c_str(), (int)impl->entries.size());
        return 0;
    } else {
        ESP_LOGW(TAG, "Filter not found: %s", normalized.c_str());
        return -1;
    }
}

int32_t file_list_get_filters(file_list_handle_t handle,
                             ext_filter_t* filters,
                             uint32_t* filter_count,
                             uint8_t* case_sensitive) {
    if (handle == NULL || filters == NULL || filter_count == NULL || case_sensitive == NULL) {
        return -1;
    }
    
    FileListImpl* impl = (FileListImpl*)handle;
    
    *filter_count = 0;
    *case_sensitive = impl->case_sensitive ? 1 : 0;
    
    for (const auto& filter : impl->filters) {
        if (filter.enabled && *filter_count < MAX_EXT_FILTERS) {
            memset(&filters[*filter_count], 0, sizeof(ext_filter_t));
            strncpy(filters[*filter_count].ext, filter.ext.c_str(), 
                    sizeof(filters[*filter_count].ext) - 1);
            filters[*filter_count].enabled = 1;
            (*filter_count)++;
        }
    }
    
    return 0;
}

int32_t file_list_clear_filters(file_list_handle_t handle) {
    if (handle == NULL) return -1;
    
    FileListImpl* impl = (FileListImpl*)handle;
    
    impl->filters.clear();
    impl->refilterEntries();
    
    ESP_LOGI(TAG, "All filters cleared: %d files", (int)impl->entries.size());
    
    return 0;
}

int32_t file_list_get_count(file_list_handle_t handle) {
    if (handle == NULL) return -1;
    FileListImpl* impl = (FileListImpl*)handle;
    return (int32_t)impl->entries.size();
}

int32_t file_list_get_by_index(file_list_handle_t handle, uint32_t index, file_info_t* info) {
    if (handle == NULL || info == NULL) return -1;
    
    FileListImpl* impl = (FileListImpl*)handle;
    if (index >= impl->entries.size()) {
        ESP_LOGW(TAG, "Index %u out of range (size: %d)", 
                 index, (int)impl->entries.size());
        return -1;
    }
    
    const FileListImpl::Entry& entry = impl->entries[index];
    
    memset(info, 0, sizeof(file_info_t));
    strncpy(info->name, entry.name.c_str(), sizeof(info->name) - 1);
    strncpy(info->path, entry.path.c_str(), sizeof(info->path) - 1);
    info->size = entry.size;
    info->modified = entry.modified;
    info->is_dir = entry.is_dir ? 1 : 0;
    
    return 0;
}

int32_t file_list_get_by_name(file_list_handle_t handle, const char* filename, file_info_t* info) {
    if (handle == NULL || filename == NULL || info == NULL) return -1;
    
    FileListImpl* impl = (FileListImpl*)handle;
    
    for (size_t i = 0; i < impl->entries.size(); i++) {
        if (impl->entries[i].name == filename) {
            return file_list_get_by_index(handle, i, info);
        }
    }
    
    ESP_LOGW(TAG, "File not found: %s", filename);
    return -1;
}

int32_t file_list_find_by_ext(file_list_handle_t handle, 
                             const char* ext, 
                             file_info_t* info,
                             uint32_t start_index) {
    if (handle == NULL || ext == NULL || info == NULL) return -1;
    
    FileListImpl* impl = (FileListImpl*)handle;
    
    std::string target_ext = FileListImpl::normalizeExt(ext);
    
    for (size_t i = start_index; i < impl->entries.size(); i++) {
        const FileListImpl::Entry& entry = impl->entries[i];
        
        bool match = impl->case_sensitive ? 
            (entry.extension == target_ext) :
            (impl->stringCompare(entry.extension, target_ext));
        
        if (match && file_list_get_by_index(handle, i, info) == 0) {
            return (int32_t)i;
        }
    }
    
    ESP_LOGD(TAG, "No file with ext: %s found", ext);
    return -1;
}

void file_list_print_all(file_list_handle_t handle) {
    if (handle == NULL) return;
    
    FileListImpl* impl = (FileListImpl*)handle;
    printf("\n=== File List ===\n");
    printf("Base path: %s\n", impl->base_path.c_str());
    printf("Recursive: %s\n", impl->recursive ? "yes" : "no");
    impl->printFilters();
    printf("Total files: %d\n", (int)impl->entries.size());
    printf("%-5s %-32s %10s %-8s %s\n", "IDX", "NAME", "SIZE", "EXT", "PATH");
    printf("----------------------------------------------------------------\n");
    
    for (size_t i = 0; i < impl->entries.size(); i++) {
        const FileListImpl::Entry& entry = impl->entries[i];
        printf("%-5d %-32s %10s %-8s %s\n", 
               (int)i, 
               entry.name.c_str(),
               entry.is_dir ? "<DIR>" : FileListImpl::formatSize(entry.size).c_str(),
               entry.extension.c_str(),
               entry.path.c_str());
    }
    
    impl->printStats();
    printf("===========================\n");
}

void file_list_destroy(file_list_handle_t handle) {
    if (handle == NULL) return;
    
    FileListImpl* impl = (FileListImpl*)handle;
    delete impl;
    ESP_LOGI(TAG, "File list destroyed");
}

} // extern "C"