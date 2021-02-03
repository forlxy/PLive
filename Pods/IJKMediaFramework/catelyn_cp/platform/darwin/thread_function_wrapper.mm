#include "../../thread_function_wrapper.h"

namespace kuaishou {
namespace kpbase {

void RunWithPlatformThreadWrapper(std::function<void()> lambda) {
  @autoreleasepool {
    lambda();
  }
}

} // namespace base
} // namespace kuaishou
