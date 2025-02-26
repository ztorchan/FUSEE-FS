#ifndef FUSEEFS_MDS_ERPC_INCLUDE_
#define FUSEEFS_MDS_ERPC_INCLUDE_

#include <cstdint>

#include <rpc.h>

#include "src/client.h"

namespace fuseefs
{

enum MDSErpcType {
  kMkdir = 0,
  kRmdir,
  kCreat,
  kUnlink,
  kStat,
  kRename,
};

struct MDSServerContext {
  uint8_t rpc_id_;
  erpc::Rpc<erpc::CTransport>* rpc_;
  int central_client_session_id_;
  Client* fusee_client_;
  void* owner_;
};

} // namespace fuseefs


#endif // FUSEEFS_MDS_ERPC_INCLUDE_