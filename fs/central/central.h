#ifndef FUSEEFS_LCKS_LCKS_INCLUDE_
#define FUSEEFS_LCKS_LCKS_INCLUDE_

#include <atomic>
#include <mutex>
#include <cstdint>
#include <unordered_set>

#include <rpc.h>

#include "fs/common/noncopyable.h"

namespace fuseefs
{

class Central : public noncopyable {
public:
  Central(std::string erpc_hostname, uint16_t erpc_port);
  ~Central();

  void MainLoop();

private:
  static void LockHandler(erpc::ReqHandle* req_handle, void* _ctx);
  static void UnlockHandler(erpc::ReqHandle* req_handle, void* _ctx);
  static void GetInodeIdGroupHandler(erpc::ReqHandle* req_handle, void* _ctx);

private:
  erpc::Nexus* nexus_;

  std::unordered_set<uint64_t> lock_set_;
  std::mutex lck_set_mtx_;
  
  std::atomic_uint64_t inode_id_group_seq_;

  bool end_;
};

} // namespace fuseefs


#endif // FUSEEFS_LCKS_LCKS_INCLUDE_