#ifndef FUSEEFS_ENGINE_ENGINE_INCLUDE_
#define FUSEEFS_ENGINE_ENGINE_INCLUDE_

#include <cstdint>
#include <string>

#include <rpc.h>

#include "src/client.h"

#include "fs/common/type.h"

#ifndef _OUT
#define _OUT
#endif

namespace fuseefs
{
  
class Engine {

public:
  Engine(const GlobalConfig& fusee_client_conf, 
         const std::string& central_erpc_hostname, uint16_t central_erpc_port,
         const std::string& local_erpc_hostname, uint16_t local_erpc_port);
  ~Engine();

  int Mkdir(const std::string& path, uint32_t uid, uint32_t gid, uint32_t mode);
  int Rmdir(const std::string& path, uint32_t uid, uint32_t gid);
  int Creat(const std::string& path, uint32_t uid, uint32_t gid, uint32_t mode);
  int Unlink(const std::string& path, uint32_t uid, uint32_t gid);
  int Stat(const std::string& path, uint32_t uid, uint32_t gid, _OUT Inode& inode);

  void InitThread(uint64_t thread_id);

private:
  static std::string GetDentryKey(const uint64_t pid, const std::string& name) {
    return std::to_string(pid) + "/" + name;
  }
  static std::string GetInodeKey(const uint64_t id) {
    return std::to_string(id);
  }
  
  void CentralLockInode(uint64_t inode_id);
  void CentralUnlockInode(uint64_t inode_id);

  void PrepareFuseeReq(const std::string& key, const void* value, uint64_t value_size, char *op_type);
  int GetInodeById(const uint64_t id, _OUT Inode& inode);
  int GetDentryByParentIdAndName(const uint64_t pid, const std::string& name, _OUT uint64_t& inode_id);
  int PathLookupToDentry(const std::string& path, uint32_t uid, uint32_t gid, _OUT uint64_t& inode_id);
  int InsertDentry(uint64_t parent_id, const std::string& name, uint64_t inode_id);
  int DeleteDentry(uint64_t parent_id, const std::string& name);
  int UpdateDentry(uint64_t parent_id, const std::string& name, uint64_t inode_id);
  int InsertInode(uint64_t id, const Inode& inode);
  int DeleteInode(uint64_t id);
  int UpdateInode(uint64_t id, const Inode& inode);

  uint64_t GetNewInodeId();

private:
  GlobalConfig fusee_client_conf_;

  static thread_local uint64_t local_thread_id;
  static thread_local std::unique_ptr<Client> fusee_client_;

  std::string central_erpc_hostname_;
  uint16_t central_erpc_port_;
  std::unique_ptr<erpc::Nexus> erpc_nexus_;
  static thread_local std::unique_ptr<erpc::Rpc<erpc::CTransport>> erpc_rpc_;
  static thread_local int erpc_session_num_;

  std::mutex inode_id_group_mtx_;
  std::atomic_uint64_t inode_id_group_;
  std::atomic_uint64_t inode_id_seq_in_group_;
};

} // namespace fuseefs


#endif  // FUSEEFS_ENGINE_ENGINE_INCLUDE_