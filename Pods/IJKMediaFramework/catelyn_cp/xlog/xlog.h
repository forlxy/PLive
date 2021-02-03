#pragma once

#ifndef DISABLE_XLOG
#include "../utility.h"

#ifdef ANDROID

#include <android/log.h>
typedef enum {
  kLevelAll = 0,
  kLevelVerbose = 0,
  kLevelDebug,    // Detailed information on the flow through the system.
  kLevelInfo,     // Interesting runtime events (startup/shutdown), should be conservative and keep to a minimum.
  kLevelWarn,     // Other runtime situations that are undesirable or unexpected, but not necessarily "wrong".
  kLevelError,    // Other runtime errors or unexpected conditions.
  kLevelFatal,    // Severe errors that cause premature termination.
  kLevelNone,     // Special level used to disable all log messages.
} TLogLevel;

#define LOG_VERBOSE(...)          kuaishou::kpbase::XLog::GetInstance()->ConsoleLog(kLevelVerbose, __VA_ARGS__);
#define LOG_DEBUG(...)            kuaishou::kpbase::XLog::GetInstance()->ConsoleLog(kLevelDebug,   __VA_ARGS__);
#define LOG_INFO(...)             kuaishou::kpbase::XLog::GetInstance()->ConsoleLog(kLevelInfo,    __VA_ARGS__);
#define LOG_WARN(...)             kuaishou::kpbase::XLog::GetInstance()->ConsoleLog(kLevelWarn,    __VA_ARGS__);
#define LOG_ERROR(...)            kuaishou::kpbase::XLog::GetInstance()->ConsoleLog(kLevelError,   __VA_ARGS__);
#define LOG_ERROR_DETAIL(...)     kuaishou::kpbase::XLog::GetInstance()->ConsoleLogDetail(kLevelError,   __func__, __LINE__,  __VA_ARGS__);
#define LOG_FATAL(...)            kuaishou::kpbase::XLog::GetInstance()->ConsoleLog(kLevelFatal, __VA_ARGS__);

#else

#include <mars/xlog/appender.h>
#include <mars/xlog/xloggerbase.h>
#include <mars/xlog/xlogger.h>

#define LOG_VERBOSE(...)  xlog2(kLevelVerbose, __VA_ARGS__);
#define LOG_DEBUG(...)    xlog2(kLevelDebug,   __VA_ARGS__);
#define LOG_INFO(...)     xlog2(kLevelInfo,    __VA_ARGS__);
#define LOG_WARN(...)     xlog2(kLevelWarn,    __VA_ARGS__);
#define LOG_ERROR(...)    xlog2(kLevelError,   __VA_ARGS__);
#define LOG_ERROR_DETAIL  LOG_ERROR
#define LOG_FATAL(...)    xlog2(kLevelFatal,   __VA_ARGS__);

#endif  //ANDROID

namespace kuaishou {
namespace kpbase {

class XLog : public NonCopyable {
 public:
  ~XLog();
  static XLog* GetInstance();
  void Init(std::string path, std::string prefix);
  void SetLogLevel(TLogLevel level);
  void SetConsoleLog(bool isOpen);
  void Flush();
  void ConsoleLog(TLogLevel level, const char* fmt, ...);
  void ConsoleLogDetail(TLogLevel level, const char* func_name, int line, const char* fmt, ...);

 private:
  XLog();
  void Start(bool isAsync, std::string& path, std::string& namePrefix, bool isCompress);
  void Stop();
  void LogImpl(TLogLevel level, const char* str);
  void LogImpl(TLogLevel level, const char* func_name, int line, const char* str);

  static const uint32_t kConsoleLogMaxLength = 1024;
  static const char* kLogTag;
  TLogLevel log_level_;
};

}
}
#else
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>

static void PRINT_TO_STDOUT(const char* format, ...) {
  va_list args;

  va_start(args, format);

  char message[1024] = "";
  int num = _vsprintf_p(message, 1024, format, args);
  va_end(args);

  OutputDebugStringA(message);
  OutputDebugStringA("\n");
}

#define LOG_VERBOSE(format, ...) PRINT_TO_STDOUT(format, __VA_ARGS__)
#define LOG_VERBOSE(format, ...) PRINT_TO_STDOUT(format, __VA_ARGS__)
#define LOG_DEBUG(format, ...) PRINT_TO_STDOUT(format, __VA_ARGS__)
#define LOG_INFO(format, ...) PRINT_TO_STDOUT(format, __VA_ARGS__)
#define LOG_WARN(format, ...) PRINT_TO_STDOUT(format, __VA_ARGS__)
#define LOG_ERROR(format, ...) PRINT_TO_STDOUT(format, __VA_ARGS__)
#define LOG_FATAL(format, ...) PRINT_TO_STDOUT(format, __VA_ARGS__)

#else
#include <stdio.h>
#define PRINT_TO_STDOUT(...) { printf(__VA_ARGS__); printf("\n"); }
#define LOG_VERBOSE(...) PRINT_TO_STDOUT(__VA_ARGS__)
#define LOG_DEBUG(...) PRINT_TO_STDOUT(__VA_ARGS__)
#define LOG_INFO(...) PRINT_TO_STDOUT(__VA_ARGS__)
#define LOG_WARN(...) PRINT_TO_STDOUT(__VA_ARGS__)
#define LOG_ERROR(...) PRINT_TO_STDOUT(__VA_ARGS__)
#define LOG_FATAL(...) PRINT_TO_STDOUT(__VA_ARGS__)
#endif  //_WIN32
#endif  //DISABLE_XLOG
