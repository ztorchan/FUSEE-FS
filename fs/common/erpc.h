#ifndef FUSEEFS_COMMON_ERPC_INCLUDE_
#define FUSEEFS_COMMON_ERPC_INCLUDE_

#include <cstdint>

#include <rpc.h>

#define kCentralErpcId (254UL)

namespace fuseefs
{

struct ContContextT {
  int finished_;
  char *buf_;
};

inline void ContFunc(void *context, void *tag) {
  auto *ret = reinterpret_cast<ContContextT *>(tag);
  ret->finished_ = 1;
}

inline void SmHandler(int, erpc::SmEventType, erpc::SmErrType, void *) {
  // do nothing
}

} // namespace fuseefs


#endif // FUSEEFS_LCKS_ERPC_INCLUDE_