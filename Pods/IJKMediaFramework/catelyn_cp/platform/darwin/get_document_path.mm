#include <Foundation/Foundation.h>
#include <sys/utsname.h>
#include <string>

#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

using namespace std;

namespace kuaishou {
namespace kpbase {

void GetDocumentPathDarwin(char* path) {
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
  strcpy(path, ".");
#else
  NSArray* paths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
  NSString* docDir = [paths objectAtIndex:0];
  strcpy(path, [docDir UTF8String]);
#endif
}

string GetDeviceModelDarwin() {
  struct utsname systemInfo;
  uname(&systemInfo);
  return string(systemInfo.machine);
}

string GetSystemOSVersion() {
  struct utsname systemInfo;
  uname(&systemInfo);
  return string(systemInfo.sysname);
}

}
}
