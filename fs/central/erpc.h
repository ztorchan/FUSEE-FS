#ifndef FUSEEFS_CENTRAL_ERPC_INCLUDE_
#define FUSEEFS_CENTRAL_ERPC_INCLUDE_

#include <cstdint>

#include <rpc.h>

namespace fuseefs
{

enum CentralErpcType {
  kLock = 0,
  kUnlock,
  kGetInodeIdGroup
};

struct CentralServerContext {
  uint8_t rpc_id_;
  erpc::Rpc<erpc::CTransport>* rpc_;
  void* owner_;
};

} // namespace fuseefs


#endif // FUSEEFS_CENTRAL_ERPC_INCLUDE_