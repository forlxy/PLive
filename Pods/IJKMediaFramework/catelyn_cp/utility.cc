#include "utility.h"
#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif
#include <climits>
#include <cstring>
#include <chrono>
#include <sstream>
#include <stdlib.h>
#include <algorithm>
#include <cctype>
#include <iomanip>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

using namespace std;

namespace {

#include "crypto/sha1.cc"
#include "crypto/hmac_sha1.c"

static void Unsigned2Str(unsigned long long val, char* str) {
  char* ptr = str;
  unsigned long long figure = 0;
  unsigned long long i = val;

  do {
    i /= 10;
    ++figure;
  } while (i);

  ptr += figure - 1;

  for (i = figure; i > 0; --i) {
    *ptr-- = '0' + (char)(val % 10);
    val /= 10;
  }
}

static bool Str2Unsigned(const char* str, unsigned long long* val) {
  const char* ptr = str;
  *val = 0;

  if (!ptr || strlen(ptr) == 0)
    return false;

  while (*ptr) {
    if (*ptr > '9' || *ptr < '0') {
      return false;
    }

    *val *= 10;
    *val += *ptr - '0';
    ++ptr;
  }

  return true;
}

bool IsIpv6(const char* ip) {
  struct addrinfo info_hints = {0};
  info_hints.ai_flags = AI_NUMERICHOST;
  struct addrinfo* info = nullptr;
  int val = getaddrinfo(ip, 0, &info_hints, &info);

  if (val != 0) {
    return false;
  }

  val = info->ai_family;
  freeaddrinfo(info);
  return val == AF_INET6;
}

static void SymmetricTransChar(char* dst, const char* src, size_t len) {
  for (size_t i = 0; i < len; ++i) {
    char temp = src[i];
    if (temp >= 0x20 && temp <= 0x7E) {
      dst[i] = 0x9E - temp;
    } else {
      dst[i] = temp;
    }
  }
}

}

namespace kuaishou {
namespace kpbase {

#pragma mark SystemUtil

uint64_t SystemUtil::GetCPUTime() {
  auto now = chrono::steady_clock::now().time_since_epoch();
  return chrono::duration_cast<chrono::milliseconds>(now).count();
}

uint64_t SystemUtil::GetEpochTime() {
  auto now = chrono::system_clock::now().time_since_epoch();
  return chrono::duration_cast<chrono::milliseconds>(now).count();
}

string SystemUtil::GetDocumentPath() {
#if (TARGET_OS_IPHONE)
  extern void GetDocumentPathDarwin(char*);
  char path[1024] = {0};
  GetDocumentPathDarwin(path);
  return string(path);
#elif __ANDROID__
  return string("/sdcard");
#elif _WIN32
#define PATH_MAX MAX_PATH
  return string(".");
#else
  return string(".");
#endif
}

string SystemUtil::GetExecFilePath() {
  char current_absolute_path[PATH_MAX];
#if _WIN32
  if (NULL == _fullpath("./", current_absolute_path, MAX_PATH))
    return "";
#else
  if (NULL == realpath("./", current_absolute_path))
    return "";
#endif
  return string(current_absolute_path);
}

string SystemUtil::GetDeviceModel() {
#if (TARGET_OS_IPHONE)
  extern string GetDeviceModelDarwin();
  return GetDeviceModelDarwin();
#elif __ANDROID__
  extern string GetDeviceModelAndroid();
  return GetDeviceModelAndroid();
#else
  return string("na");
#endif
}

string SystemUtil::GetSystemOSVersion() {
#if (TARGET_OS_IPHONE) || defined(__ANDROID__)
  extern string GetSystemOSVersion();
  return GetSystemOSVersion();
#else
  return string("na");
#endif
}

string SystemUtil::GetTimeString(uint64_t ms_epoch, int timezone, bool with_delimiter) {
  ms_epoch += timezone * 3600000;
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint16_t millisecond;
  int64_t time = ms_epoch / 1000;
  int64_t yy, mm, dd, hh, nn, ss;
  int64_t a, b, c, d, e, m, i;
  ss = time % 60;
  i = time / 60;
  nn = i % 60;
  i = i / 60;
  hh = i % 24;
  i = i / 24;
  a = i + 2472632;
  b = (4 * a + 3) / 146097;
  c = (-b * 146097) / 4 + a;
  d = (4 * c + 3) / 1461;
  e = (-1461 * d) / 4 + c;
  m = (5 * e + 2) / 153;
  dd = -(153 * m + 2) / 5 + e + 1;
  mm = (-m / 10) * 12 + m + 3;
  yy = b * 100 + d - 4800 + (m / 10);

  year = static_cast<uint16_t>(yy);
  month = static_cast<uint8_t>(mm);
  day = static_cast<uint8_t>(dd);
  hour = static_cast<uint8_t>(hh);
  minute = static_cast<uint8_t>(nn);
  second = static_cast<uint8_t>(ss);
  millisecond = static_cast<uint16_t>(ms_epoch % 1000);

  char date_str[128] = {0};
  if (with_delimiter) {
    sprintf(date_str, "%4d-%02d-%02dT%02d:%02d:%02d.%03d%c%02d:00", year, month, day, hour, minute, second, millisecond, (timezone >= 0 ? '+' : '-'), timezone);
  } else {
    sprintf(date_str, "%4d%02d%02d%02d%02d%02d%03d", year, month, day, hour, minute, second, millisecond);
  }
  return string(date_str);
}

string SystemUtil::GetDayString(uint64_t ms_epoch, int timezone) {
  ms_epoch += timezone * 3600000;
  uint16_t year;
  uint8_t month;
  uint8_t day;

  int64_t time = ms_epoch / 1000;
  int64_t yy, mm, dd;
  int64_t a, b, c, d, e, m, i;
  i = time / 60;
  i = i / 60;
  i = i / 24;
  a = i + 2472632;
  b = (4 * a + 3) / 146097;
  c = (-b * 146097) / 4 + a;
  d = (4 * c + 3) / 1461;
  e = (-1461 * d) / 4 + c;
  m = (5 * e + 2) / 153;
  dd = -(153 * m + 2) / 5 + e + 1;
  mm = (-m / 10) * 12 + m + 3;
  yy = b * 100 + d - 4800 + (m / 10);

  year = static_cast<uint16_t>(yy);
  month = static_cast<uint8_t>(mm);
  day = static_cast<uint8_t>(dd);

  char date_str[128] = {0};
  sprintf(date_str, "%4d-%02d-%02d", year, month, day);
  return string(date_str);
}

string SystemUtil::GetHourString(uint64_t ms_epoch, int timezone) {
  ms_epoch += timezone * 3600000;
  uint16_t year;
  uint8_t month;
  uint8_t day;
  uint8_t hour;
  int64_t time = ms_epoch / 1000;
  int64_t yy, mm, dd, hh;
  int64_t a, b, c, d, e, m, i;
  i = time / 60;
  i = i / 60;
  hh = i % 24;
  i = i / 24;
  a = i + 2472632;
  b = (4 * a + 3) / 146097;
  c = (-b * 146097) / 4 + a;
  d = (4 * c + 3) / 1461;
  e = (-1461 * d) / 4 + c;
  m = (5 * e + 2) / 153;
  dd = -(153 * m + 2) / 5 + e + 1;
  mm = (-m / 10) * 12 + m + 3;
  yy = b * 100 + d - 4800 + (m / 10);

  year = static_cast<uint16_t>(yy);
  month = static_cast<uint8_t>(mm);
  day = static_cast<uint8_t>(dd);
  hour = static_cast<uint8_t>(hh);

  char date_str[128] = {0};
  sprintf(date_str, "%4d-%02d-%02d-%02d", year, month, day, hour);
  return string(date_str);
}

#pragma mark StringUtil

string StringUtil::Int2Str(long long val) {
  char str[64] = {0};

  if (val < 0) {
    str[0] = '-';
    Unsigned2Str(static_cast<unsigned long long>(-val), str + 1);
  } else {
    Unsigned2Str(static_cast<unsigned long long>(val), str);
  }

  return string(str);
}

string StringUtil::Uint2Str(unsigned long long val) {
  char str[64] = {0};
  Unsigned2Str(static_cast<unsigned long long>(val), str);
  return string(str);
}

Nullable<long long> StringUtil::Str2Int(string str) {
  unsigned long long val = 0;
  const char* ptr = str.c_str();
  bool nagitive = false;

  if (*ptr == '-') {
    nagitive = true;
    ++ptr;
  } else if (*ptr == '+') {
    ++ptr;
  }

  bool ret = Str2Unsigned(ptr, &val);

  if (!ret) {
    return Nullable<long long>();
  }

  long long result = 0;

  if (!nagitive) {
    result = static_cast<long long>(val);
  } else {
    result = -(static_cast<long long>(val));
  }

  return Nullable<long long>(result);
}

Nullable<unsigned long long> StringUtil::Str2Uint(string str) {
  unsigned long long val = 0;
  const char* ptr = str.c_str();

  if (*ptr == '+') {
    ++ptr;
  }

  bool ret = Str2Unsigned(ptr, &val);

  if (!ret) {
    return Nullable<unsigned long long>();
  }

  return Nullable<unsigned long long>(val);
}

string StringUtil::GetHumanReadableString(int64_t val) {
  static const char UNITS[] = {'\0', 'k', 'm', 'g', 't'};
  static const size_t MAX_UNIT_INDEX = sizeof(UNITS) / sizeof(char);

  std::stringstream ss;
  size_t unit_index = 0;
  int64_t left = 0;
  while (val > 1024 && unit_index < MAX_UNIT_INDEX) {
    left = val % 1024;
    val >>= 10;
    ++unit_index;
  }

  ss << val;
  if (left > 0) {
    ss << '.';
    left = static_cast<int64_t>(left / 1.024f);
    ss << left / 100;
    left -= (left / 100 * 100);
    ss << left / 10;
  }
  if (unit_index > 0) {
    ss << UNITS[unit_index];
  }
  return ss.str();
}

string StringUtil::ConcealStr(const string& src) {
  string dst = src;
  SymmetricTransChar((char*)dst.c_str(), src.c_str(), src.length());
  return dst;
}

string StringUtil::UnConcealStr(const string& src) {
  string dst = src;
  SymmetricTransChar((char*)dst.c_str(), src.c_str(), src.length());
  return dst;
}

static inline void Ltrim(std::string& s) {
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int c) {
    return !std::isspace(c);
  }));
}

static inline void Rtrim(std::string& s) {
  s.erase(std::find_if(s.rbegin(), s.rend(), [](int c) {
    return !std::isspace(c);
  }).base(), s.end());
}

string StringUtil::Trim(const string& src) {
  string trimmed = src;
  Ltrim(trimmed);
  Rtrim(trimmed);
  return trimmed;
}

bool StringUtil::StartWith(const string& origin, const string& substr) {
  if (origin.size() < substr.size()) {
    return false;
  }
  return 0 == origin.compare(0, substr.length(), substr);
}

bool StringUtil::EndWith(const string& origin, const string& substr) {
  if (origin.size() < substr.size()) {
    return false;
  }
  return 0 == origin.compare(origin.size() - substr.size(), substr.length(), substr);
}

vector<string> StringUtil::Split(const string& s, const string& pattern) {
  std::string::size_type pos;
  std::vector<std::string> result;
  if (pattern.empty()) {
    return result;
  }
  std::string str = s + pattern;
  size_t size = str.size();

  for (size_t i = 0; i < size;) {
    pos = str.find(pattern, i);
    if (pos < size) {
      std::string s0 = str.substr(i, pos - i);
      result.push_back(s0);
      i = pos + pattern.size();
    } else {
      break;
    }
  }
  return result;
}

string StringUtil::UrlEncode(const string& str) {
  ostringstream escaped;
  escaped.fill('0');
  escaped << hex;

  for (string::const_iterator i = str.begin(), n = str.end(); i != n; ++i) {
    string::value_type c = (*i);

    // Keep alphanumeric and other accepted characters intact
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << c;
      continue;
    }

    // Any other characters are percent-encoded
    escaped << uppercase;
    escaped << '%' << setw(2) << int((unsigned char) c);
    escaped << nouppercase;
  }

  return escaped.str();
}

string StringUtil::HmacSha1(const void* data, size_t data_len, const void* key, size_t key_len) {
  uint8_t sha1_out[20] = {0};
  size_t out_len = sizeof(sha1_out);
  hmac_sha1(static_cast<const uint8_t*>(key), key_len, static_cast<const uint8_t*>(data), data_len, sha1_out, &out_len);

  stringstream ss;
  for (size_t i = 0; i < out_len; ++i) {
    ss << std::setfill('0') << std::setw(2) << std::hex << static_cast<unsigned>(sha1_out[i]);
  }

  return ss.str();
}

string StringUtil::Replace(string& str, const string& old_value, const string& new_value) {
  for (string::size_type pos(0); pos != string::npos; pos += new_value.length()) {
    if ((pos = str.find(old_value, pos)) != string::npos)
      str.replace(pos, old_value.length(), new_value);
    else {
      break;
    }
  }
  return str;
}

#pragma mark SocketUtil

bool SocketUtil::FillInSockaddr(const char* ip, uint16_t port, void* addr, size_t* addrlen) {
  bool is_ipv6 = IsIpv6(ip);

  if (is_ipv6) {
    sockaddr_in6* addrIn = static_cast<sockaddr_in6*>(addr);
    memset(addrIn, 0, sizeof(sockaddr_in6));

    if (inet_pton(AF_INET6, ip, &(addrIn->sin6_addr)) != 1) {
      return false;
    }

    addrIn->sin6_family = AF_INET6;
    addrIn->sin6_port = htons(port);

    if (addrlen)
      *addrlen = sizeof(sockaddr_in6);
  } else {
    sockaddr_in* addrIn = reinterpret_cast<sockaddr_in*>(addr);
    memset(addrIn, 0, sizeof(sockaddr_in));

    if (inet_pton(AF_INET, ip, &(addrIn->sin_addr)) != 1) {
      return false;
    }

    addrIn->sin_family = AF_INET;
    addrIn->sin_port = htons(port);

    if (addrlen)
      *addrlen = sizeof(sockaddr_in);
  }

  return true;
}

pair<string, uint16_t> SocketUtil::GetAddress(const struct sockaddr* addr) {
  char ip[INET6_ADDRSTRLEN] = {0};
  uint16_t port = 0;

  if (addr->sa_family == AF_INET) {
    const struct sockaddr_in* addr_v4 = reinterpret_cast<const struct sockaddr_in*>(addr);
#if _WIN32
    inet_ntop(AF_INET, (void*)&addr_v4->sin_addr, ip, INET_ADDRSTRLEN);
#else
    inet_ntop(AF_INET, &addr_v4->sin_addr, ip, INET_ADDRSTRLEN);
#endif
    port = ntohs(addr_v4->sin_port);
  } else if (addr->sa_family == AF_INET6) {
    const struct sockaddr_in6* addr_v6 = reinterpret_cast<const struct sockaddr_in6*>(addr);
#ifdef _WIN32
    inet_ntop(AF_INET6, (void*)&addr_v6->sin6_addr, ip, INET6_ADDRSTRLEN);
#else
    inet_ntop(AF_INET6, &addr_v6->sin6_addr, ip, INET6_ADDRSTRLEN);
#endif
    port = ntohs(addr_v6->sin6_port);
  }

  return {string(ip), port};
}

}
}
