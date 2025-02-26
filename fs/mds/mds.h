#ifndef FUSEEFS_MDS_MDS_INCLUDE_
#define FUSEEFS_MDS_MDS_INCLUDE_

#include <mutex>
#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include <rpc.h>
#include "src/client.h"

#include "fs/common/type.h"
#include "fs/common/noncopyable.h"
#include "fs/mds/erpc.h"

#ifndef _OUT
#define _OUT
#endif

namespace fuseefs
{

class MDS : public noncopyable {
public:
  MDS(std::string erpc_hostname, uint16_t erpc_port,  // erpc server
      std::string central_erpc_hostname, uint16_t central_erpc_port,  // central erpc client
      const GlobalConfig& fusee_client_conf);
  ~MDS();

  void MainLoop();

private:
  uint64_t GetNewInodeId(MDSServerContext* ctx);

private:
  static std::string GetDentryKey(const uint64_t pid, const std::string& name) {
    return std::to_string(pid) + "/" + name;
  }
  static std::string GetInodeKey(const uint64_t id) {
    return std::to_string(id);
  }
  static int GetInodeById(const uint64_t id, MDSServerContext* ctx, _OUT Inode& inode);
  static int GetDentryByParentIdAndName(const uint64_t pid, const std::string& name, 
                                        MDSServerContext* ctx, _OUT uint64_t& inode_id);
  static int PathLookupToDentry(const std::string& path, uint32_t uid, uint32_t gid, MDSServerContext* ctx,
                                _OUT uint64_t& inode_id);
  
  static void CentralLockInode(uint64_t inode_id, MDSServerContext* ctx);
  static void CentralUnlockInode(uint64_t inode_id, MDSServerContext* ctx);

  static int InsertDentry(uint64_t parent_id, const std::string& name, uint64_t inode_id, MDSServerContext* ctx);
  static int DeleteDentry(uint64_t parent_id, const std::string& name, MDSServerContext* ctx);
  static int UpdateDentry(uint64_t parent_id, const std::string& name, uint64_t inode_id, MDSServerContext* ctx);
  static int InsertInode(uint64_t id, const Inode& inode, MDSServerContext* ctx);
  static int DeleteInode(uint64_t id, MDSServerContext* ctx);
  static int UpdateInode(uint64_t id, const Inode& inode, MDSServerContext* ctx);

  static void MkdirHandler(erpc::ReqHandle* req_handle, void* _ctx);
  static void RmdirHandler(erpc::ReqHandle* req_handle, void* _ctx);
  static void CreatHandler(erpc::ReqHandle* req_handle, void* _ctx);
  static void UnlinkHandler(erpc::ReqHandle* req_handle, void* _ctx);
  static void StatHandler(erpc::ReqHandle* req_handle, void* _ctx);
  // static void RenameHandler(erpc::ReqHandle* req_handle, void* _ctx);

  // thread loop
  static void ThreadLoopOnOneCore(MDS* mds, uint64_t core_id);

private:
  GlobalConfig fusee_client_conf_;
  std::string central_erpc_hostname_;
  uint16_t central_erpc_port_;

  erpc::Nexus* nexus_;
  std::vector<std::thread> threads_;

  std::mutex inode_id_group_mtx_;
  std::atomic_uint64_t inode_id_group_;
  std::atomic_uint64_t inode_id_seq_in_group_;

  bool end_;
};

} // namespace fuseefs


#endif // FUSEEFS_MDS_MDS_INCLUDE_