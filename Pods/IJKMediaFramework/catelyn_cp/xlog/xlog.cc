#include <string>
#include "xlog.h"

namespace kuaishou {
namespace kpbase {

XLog::XLog() {
  log_level_ = kLevelNone;
}

void XLog::Init(std::string path, std::string prefix) {
#ifndef ANDROID
  std::string log_path    = SystemUtil::GetDocumentPath() + "/" + path;
  std::string name_prefix = prefix;
  Start(true, log_path, name_prefix, false);
#endif
}

XLog::~XLog() {
  Stop();
}

XLog* XLog::GetInstance() {
  static XLog logger_instance;
  return &logger_instance;
}

void XLog::SetLogLevel(TLogLevel level) {
  log_level_ = level;
#ifndef ANDROID
  xlogger_SetLevel(level);
#endif
}

void XLog::SetConsoleLog(bool isOpen) {
#ifndef ANDROID
  appender_set_console_log(isOpen);
#endif
}

void XLog::Start(bool isAsync, std::string& path, std::string& namePrefix, bool isCompress) {
#ifndef ANDROID
  appender_open(isAsync ? kAppednerAsync : kAppednerSync, path.c_str(), namePrefix.c_str(), isCompress);
#endif
}

void XLog::Stop() {
#ifndef ANDROID
  appender_close();
#endif
}

void XLog::Flush() {
#ifndef ANDROID
  appender_flush();
#endif
}

void XLog::ConsoleLog(TLogLevel level, const char* fmt, ...) {
  if (level >= log_level_) {
    char buf[kConsoleLogMaxLength] = {0};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, kConsoleLogMaxLength, fmt, args);
    va_end(args);
    LogImpl(level, buf);
  }
}


void XLog::ConsoleLogDetail(TLogLevel level, const char* func_name, int line, const char* fmt, ...) {
  if (level >= log_level_) {
    char buf[kConsoleLogMaxLength] = {0};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, kConsoleLogMaxLength, fmt, args);
    va_end(args);
    LogImpl(level, func_name, line, buf);
  }
}

const char* XLog::kLogTag = "xlog";
void XLog::LogImpl(TLogLevel level, const char* str) {
#ifdef ANDROID
  __android_log_print(level + 2, kLogTag, "%s", str);
#else
  printf("%s\n", str);
#endif
}

void XLog::LogImpl(TLogLevel level, const char* func_name, int line, const char* str) {
#ifdef ANDROID
  __android_log_print(level + 2, kLogTag, "[%s][line:%4d]%s", func_name, line, str);
#else
  printf("%s\n", str);
#endif
}


}
}
