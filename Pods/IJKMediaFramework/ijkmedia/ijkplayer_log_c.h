#pragma once

typedef enum {
    kLevelAll = 0,
    kLevelDebug = 0,    // Detailed information on the flow through the system.
    kLevelInfo,     // Interesting runtime events (startup/shutdown), should be conservative and keep to a minimum.
    kLevelWarn,     // Other runtime situations that are undesirable or unexpected, but not necessarily "wrong".
    kLevelError,    // Other runtime errors or unexpected conditions.
    kLevelNone,     // Special level used to disable all log messages.
} LogLevel;

#ifdef __cplusplus
extern "C" {
#endif

void LOG_initIjkplayerLog(LogLevel log_level, bool is_console_enable, bool is_file_enable, char* file_path, uint32_t max_file_size, uint32_t max_file_num, void (*log_cb)(int level, const char* log, size_t len));
void LOG_callback(int level, const char* fmt, va_list vl);

#ifdef __cplusplus
}
#endif