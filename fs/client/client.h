#ifndef FUSEEFS_CLIENT_CLIENT_INCLUDE_
#define FUSEEFS_CLIENT_CLIENT_INCLUDE_

#include <vector>
#include <atomic>

#include <rpc.h>

namespace fuseefs
{

struct ClientContext {
  uint32_t uid_;
  uint32_t gid_;

  std::string client_erpc_hostname_;
  uint16_t    client_erpc_ctl_base_port_;

  erpc::Nexus* nexus_;
  erpc::Rpc<erpc::CTransport>* rpc_;
  std::vector<std::vector<int>> mds_sessions_;
  std::atomic_uint64_t __ready_sessions_;
};

} // namespace fuseefs

#endif // FUSEEFS_CLIENT_CLIENT_INCLUDE_