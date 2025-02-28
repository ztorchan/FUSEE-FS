#include <fs/common/thread.h>
#include <fs/common/erpc.h>
#include <fs/common/path.h>
#include <fs/common/meta.h>
#include <fs/central/erpc.h>
#include <fs/engine/engine.h>

namespace fuseefs
{
thread_local uint64_t Engine::local_thread_id;
thread_local std::unique_ptr<Client> Engine::fusee_client_;
thread_local std::unique_ptr<erpc::Rpc<erpc::CTransport>> Engine::erpc_rpc_;
thread_local int Engine::erpc_session_num_;

Engine::Engine(const GlobalConfig& fusee_client_conf, 
               const std::string& central_erpc_hostname, uint16_t central_erpc_port,
               const std::string& local_erpc_hostname, uint16_t local_erpc_port)
  : fusee_client_conf_(fusee_client_conf)
  , central_erpc_hostname_(central_erpc_hostname)
  , central_erpc_port_(central_erpc_port)
  , erpc_nexus_(nullptr)
  , inode_id_group_(0)
  , inode_id_seq_in_group_(0) {
  erpc_nexus_ = std::make_unique<erpc::Nexus>(local_erpc_hostname + ":" + std::to_string(local_erpc_port));
}

Engine::~Engine() {}

void Engine::InitThread(uint64_t thread_id) {
  local_thread_id = thread_id;
  erpc_rpc_ = std::make_unique<erpc::Rpc<erpc::CTransport>>(erpc_nexus_.get(), nullptr, thread_id, SmHandler);
  erpc_session_num_ = erpc_rpc_->create_session(central_erpc_hostname_ + ":" + std::to_string(central_erpc_port_), kCentralErpcId);
  GlobalConfig thread_local_fusee_client_conf = fusee_client_conf_;
  thread_local_fusee_client_conf.server_id = thread_id;
  thread_local_fusee_client_conf.main_core_id += thread_id * 2;
  thread_local_fusee_client_conf.poll_core_id += thread_id * 2;
  fusee_client_ = std::make_unique<Client>(&fusee_client_conf_);

  if (thread_id == 0) {
    printf("[Engine.InitThread] create root inode\n");
    // create root inode
    Inode root_inode;
    root_inode.id = kRootInodeId;
    root_inode.mode = S_IFDIR | 0755;
    root_inode.uid = 0;
    root_inode.gid = 0;
    root_inode.size = 0;
    root_inode.atime = root_inode.mtime = root_inode.ctime = time(nullptr);
    int ret = InsertInode(kRootInodeId, root_inode);
    inode_id_seq_in_group_.store(1);
    printf("[Engine.InitThread] create root inode done: %d\n", ret);
  }
}

uint64_t Engine::GetNewInodeId() {
  uint64_t new_inode_seq = inode_id_seq_in_group_.fetch_add(1);
  while (new_inode_seq >= kInodeIdGroupSize) {
    // get new group
    std::unique_lock<std::mutex> lck(inode_id_group_mtx_);
    if ((new_inode_seq = inode_id_seq_in_group_.fetch_add(1)) >= kInodeIdGroupSize) {
      // double check
      break;
    }
    erpc::MsgBuffer req = erpc_rpc_->alloc_msg_buffer(16);
    erpc::MsgBuffer rsp = erpc_rpc_->alloc_msg_buffer(sizeof(uint64_t));
    ContContextT cctx = {0, reinterpret_cast<char*>(rsp.buf_)};
    erpc_rpc_->enqueue_request(erpc_session_num_, kGetInodeIdGroup, &req, &rsp, ContFunc, &cctx);
    while (cctx.finished_ == 0) {
      erpc_rpc_->run_event_loop(1000);
    }
    inode_id_group_.store(*reinterpret_cast<uint64_t*>(rsp.buf_));
    inode_id_seq_in_group_.store(0);
    erpc_rpc_->free_msg_buffer(req);
    erpc_rpc_->free_msg_buffer(rsp);
    new_inode_seq = inode_id_seq_in_group_.fetch_add(1);
  }
  return inode_id_group_.load() * kInodeIdGroupSize + new_inode_seq;
}

void Engine::CentralLockInode(uint64_t inode_id) {
  // bool locked = false;
  // erpc::MsgBuffer req = erpc_rpc_->alloc_msg_buffer(sizeof(uint64_t) * kMaxLockInodeNum);
  // erpc::MsgBuffer rsp = erpc_rpc_->alloc_msg_buffer(1);
  // reinterpret_cast<uint64_t*>(req.buf_)[0] = inode_id;
  // while (!locked) {
  //   ContContextT cctx = {0, reinterpret_cast<char*>(rsp.buf_)};
  //   erpc_rpc_->enqueue_request(erpc_session_num_, kLock, &req, &rsp, ContFunc, &cctx);
  //   while (cctx.finished_ == 0) {
  //     erpc_rpc_->run_event_loop(1000);
  //   }
  //   if (rsp.buf_[0] == 0) {
  //     locked = true;
  //   }
  // }
  // erpc_rpc_->free_msg_buffer(req);
  // erpc_rpc_->free_msg_buffer(rsp);
}

void Engine::CentralUnlockInode(uint64_t inode_id) {
  // erpc::MsgBuffer req = erpc_rpc_->alloc_msg_buffer(sizeof(uint64_t) * kMaxLockInodeNum);
  // erpc::MsgBuffer rsp = erpc_rpc_->alloc_msg_buffer(1);
  // reinterpret_cast<uint64_t*>(req.buf_)[0] = inode_id;
  // ContContextT cctx = {0, reinterpret_cast<char*>(rsp.buf_)};
  // erpc_rpc_->enqueue_request(erpc_session_num_, kUnlock, &req, &rsp, ContFunc, &cctx);
  // while (cctx.finished_ == 0) {
  //   erpc_rpc_->run_event_loop(1000);
  // }
  // erpc_rpc_->free_msg_buffer(req);
  // erpc_rpc_->free_msg_buffer(rsp);
}

void Engine::PrepareFuseeReq(const std::string& key, const void* value, uint64_t value_size, char *op_type) {
  if (fusee_client_->kv_info_list_ != NULL) {
    free(fusee_client_->kv_info_list_);
  }
  if (fusee_client_->kv_req_ctx_list_ != NULL) {
    delete fusee_client_->kv_req_ctx_list_;
  }
  fusee_client_->kv_info_list_ = (KVInfo *)malloc(sizeof(KVInfo));
  fusee_client_->kv_req_ctx_list_ = new KVReqCtx;
  memset(fusee_client_->kv_info_list_, 0, sizeof(KVInfo));

  uint64_t input_buf_ptr = (uint64_t)fusee_client_->get_input_buf();
  uint32_t all_len = sizeof(KVLogHeader) + key.size() + value_size + sizeof(KVLogTail); 
  void* key_st_addr = (void *)(input_buf_ptr + sizeof(KVLogHeader));
  void* value_st_addr = (void *)(input_buf_ptr + sizeof(KVLogHeader) + key.size());
  memcpy(key_st_addr, key.c_str(), key.size());
  if (value_size != 0) {
    memcpy(value_st_addr, value, value_size);
  }
  fusee_client_->kv_info_list_->key_len = key.size();
  fusee_client_->kv_info_list_->value_len = value_size;
  fusee_client_->kv_info_list_->l_addr = fusee_client_->get_input_buf();
  fusee_client_->kv_info_list_->lkey = fusee_client_->get_input_buf_lkey();

  KVLogHeader *kv_log_header = (KVLogHeader *)input_buf_ptr;
  kv_log_header->key_length = key.size();
  kv_log_header->value_length = value_size;

  KVLogTail *kv_log_tail = (KVLogTail *)(input_buf_ptr + sizeof(KVLogHeader) + key.size() + value_size);
  kv_log_tail->op = KV_OP_INSERT;

  fusee_client_->init_kv_req_ctx(fusee_client_->kv_req_ctx_list_, fusee_client_->kv_info_list_, op_type);
  fusee_client_->init_kvreq_space(local_thread_id, 0, 1);
  fusee_client_->kv_req_ctx_list_->coro_id = local_thread_id;
}

int Engine::GetInodeById(const uint64_t id, _OUT Inode& inode) {
  std::string key = GetInodeKey(id);
  bool should_stop = false;
  PrepareFuseeReq(key, nullptr, 0, "READ");
  fusee_client_->kv_req_ctx_list_->should_stop = &should_stop;
  void* value_addr = fusee_client_->kv_search_sync(fusee_client_->kv_req_ctx_list_);
  if (value_addr == nullptr) {
    return -ENOENT;
  }
  inode = *reinterpret_cast<Inode*>(value_addr);
  return 0;
}

int Engine::GetDentryByParentIdAndName(const uint64_t pid, const std::string& name, _OUT uint64_t& inode_id) {
  std::string key = GetDentryKey(pid, name);
  bool should_stop = false;
  PrepareFuseeReq(key, nullptr, 0, "READ");
  fusee_client_->kv_req_ctx_list_->should_stop = &should_stop;
  void* value_addr = fusee_client_->kv_search_sync(fusee_client_->kv_req_ctx_list_);
  if (value_addr == nullptr) {
    return -ENOENT;
  }
  inode_id = *reinterpret_cast<uint64_t*>(value_addr);
  return 0;
}

int Engine::PathLookupToDentry(const std::string& path, uint32_t uid, uint32_t gid, _OUT uint64_t& inode_id) {
  uint64_t cur_inode_id, cur_parent_inode_id;
  Inode inode;
  std::vector<std::string> components = SplitPath(FormatPath(path));
  if (components.empty()) {
    inode_id = kRootInodeId;
    return 0;
  }
  // get root inode
  cur_parent_inode_id = kRootInodeId;
  int ret = GetInodeById(cur_parent_inode_id, inode);
  if (ret != 0) {
    return ret;
  }
  if (!CheckReadPermission(inode, uid, gid)) {
    return -EACCES;
  }
  // path lookup
  for (size_t i = 0; i < components.size(); i++) {
    const std::string& name = components[i];
    // get inode id
    ret = GetDentryByParentIdAndName(cur_parent_inode_id, name, cur_inode_id);
    if (ret != 0) {
      return ret;
    }
    if (i == components.size() - 1) {
      break;
    }
    // get inode and check
    ret = GetInodeById(cur_inode_id, inode);
    if (ret != 0) {
      return ret;
    }
    if (!CheckReadPermission(inode, uid, gid)) {
      return -EACCES;
    }
    if (!IsDir(inode.mode)) {
      return -ENOTDIR;
    }
    cur_parent_inode_id = cur_inode_id;
  }
  inode_id = cur_inode_id;
  return 0;
}

int Engine::InsertDentry(uint64_t parent_id, const std::string& name, uint64_t inode_id) {
  std::string key = GetDentryKey(parent_id, name);
  PrepareFuseeReq(key, &inode_id, sizeof(uint64_t), "INSERT");
  int ret = fusee_client_->kv_insert_sync(fusee_client_->kv_req_ctx_list_);
  if (ret == KV_OPS_FAIL_RETURN) {
    return -EIO;
  }
  return 0;
}

int Engine::DeleteDentry(uint64_t parent_id, const std::string& name) {
  std::string key = GetDentryKey(parent_id, name);
  PrepareFuseeReq(key, nullptr, 0, "DELETE");
  int ret = fusee_client_->kv_delete_sync(fusee_client_->kv_req_ctx_list_);
  if (ret == KV_OPS_FAIL_RETURN) {
    return -EIO;
  }
  return 0;
}

int Engine::UpdateDentry(uint64_t parent_id, const std::string& name, uint64_t inode_id) {
  std::string key = GetDentryKey(parent_id, name);
  PrepareFuseeReq(key, &inode_id, sizeof(uint64_t), "UPDATE");
  int ret = fusee_client_->kv_update_sync(fusee_client_->kv_req_ctx_list_);
  if (ret == KV_OPS_FAIL_RETURN) {
    return -EIO;
  }
  return 0;
}

int Engine::InsertInode(uint64_t id, const Inode& inode) {
  std::string key = GetInodeKey(id);
  PrepareFuseeReq(key, &inode, sizeof(Inode), "INSERT");
  int ret = fusee_client_->kv_insert_sync(fusee_client_->kv_req_ctx_list_);
  if (ret == KV_OPS_FAIL_RETURN) {
    return -EIO;
  }
  return 0;
}

int Engine::DeleteInode(uint64_t id) {
  std::string key = GetInodeKey(id);
  PrepareFuseeReq(key, nullptr, 0, "DELETE");
  int ret = fusee_client_->kv_delete_sync(fusee_client_->kv_req_ctx_list_);
  if (ret == KV_OPS_FAIL_RETURN) {
    return -EIO;
  }
  return 0;
}

int Engine::UpdateInode(uint64_t id, const Inode& inode) {
  std::string key = GetInodeKey(id);
  PrepareFuseeReq(key, &inode, sizeof(Inode), "UPDATE");
  int ret = fusee_client_->kv_update_sync(fusee_client_->kv_req_ctx_list_);
  if (ret == KV_OPS_FAIL_RETURN) {
    return -EIO;
  }
  return 0;
}

int Engine::Mkdir(const std::string& path, uint32_t uid, uint32_t gid, uint32_t mode) {
  auto [parent_path, name] = SeparateLastComponent(FormatPath(path));
  uint64_t parent_inode_id, new_inode_id;
  Inode parent_inode, new_inode;
  int ret = PathLookupToDentry(parent_path, uid, gid, parent_inode_id);
  if (ret != 0) {
    return ret;
  }
  CentralLockInode(parent_inode_id);
  // get parent inode
  ret = GetInodeById(parent_inode_id, parent_inode);
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id);
    return ret;
  }
  if (!IsDir(parent_inode.mode)) {
    CentralUnlockInode(parent_inode_id);
    return -ENOTDIR;
  }
  // check permission
  if (!CheckWritePermission(parent_inode, uid, gid)) {
    CentralUnlockInode(parent_inode_id);
    return -EACCES;
  }
  // check if exists
  ret = GetDentryByParentIdAndName(parent_inode_id, name, new_inode_id);
  if (ret == 0) {
    CentralUnlockInode(parent_inode_id);
    return -EEXIST;
  }
  // create new inode
  new_inode.id = GetNewInodeId();
  new_inode.mode = S_IFDIR | mode;
  new_inode.uid = uid;
  new_inode.gid = gid;
  new_inode.size = 0;
  new_inode.atime = new_inode.mtime = new_inode.ctime = time(nullptr);
  new_inode.children = 0;
  // insert new inode
  ret = InsertInode(new_inode.id, new_inode);
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id);
    return ret;
  }
  // insert new dentry
  ret = InsertDentry(parent_inode_id, name, new_inode.id);
  if (ret != 0) {
    DeleteInode(new_inode.id);
    CentralUnlockInode(parent_inode_id);
    return ret;
  }
  // update parent inode
  parent_inode.children++;
  ret = UpdateInode(parent_inode_id, parent_inode);
  if (ret != 0) {
    DeleteInode(new_inode.id);
    DeleteDentry(parent_inode_id, name);
    CentralUnlockInode(parent_inode_id);
    return ret;
  }
  CentralUnlockInode(parent_inode_id);
  return 0;
}

int Engine::Rmdir(const std::string& path, uint32_t uid, uint32_t gid) {
  // get parent id
  auto [parent_path, name] = SeparateLastComponent(FormatPath(path));
  uint64_t parent_inode_id, target_inode_id;
  Inode parent_inode, target_inode;
  int ret = PathLookupToDentry(parent_path, uid, gid, parent_inode_id);
  if (ret != 0) {
    return ret;
  }
  // lock parent inode
  CentralLockInode(parent_inode_id);
  // get parent inode
  ret = GetInodeById(parent_inode_id, parent_inode);
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id);
    return ret;
  }
  if (!IsDir(parent_inode.mode)) {
    CentralUnlockInode(parent_inode_id);
    return -ENOTDIR;
  }
  // check permission
  if (!CheckWritePermission(parent_inode, uid, gid)) {
    CentralUnlockInode(parent_inode_id);
    return -EACCES;
  }
  // check if the target dir exists
  ret = GetDentryByParentIdAndName(parent_inode_id, name, target_inode_id);
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id);
    return ret;
  }
  // Lock the target inode
  CentralLockInode(target_inode_id);
  // get target inode
  ret = GetInodeById(target_inode_id, target_inode);
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id);
    CentralUnlockInode(target_inode_id);
    return ret;
  }
  if (!IsDir(target_inode.mode)) {
    CentralUnlockInode(parent_inode_id);
    CentralUnlockInode(target_inode_id);
    return -ENOTDIR;
  }
  if (target_inode.children > 0) {
    CentralUnlockInode(parent_inode_id);
    CentralUnlockInode(target_inode_id);
    return -ENOTEMPTY;
  }
  // delete target inode
  parent_inode.children--;
  ret = DeleteInode(target_inode_id);
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id);
    CentralUnlockInode(target_inode_id);
    return ret;
  }
  ret = DeleteDentry(parent_inode_id, name); 
  if (ret != 0) {
    InsertInode(target_inode_id, target_inode);
    CentralUnlockInode(parent_inode_id);
    CentralUnlockInode(target_inode_id);
    return ret;
  }
  ret = UpdateInode(parent_inode_id, parent_inode);
  if (ret != 0) {
    InsertInode(target_inode_id, target_inode);
    InsertDentry(parent_inode_id, name, target_inode_id);
    CentralUnlockInode(parent_inode_id);
    CentralUnlockInode(target_inode_id);
    return ret;
  }
  // unlock
  CentralUnlockInode(target_inode_id);
  CentralUnlockInode(parent_inode_id);
  return 0;
}

int Engine::Creat(const std::string& path, uint32_t uid, uint32_t gid, uint32_t mode) {
  auto [parent_path, name] = SeparateLastComponent(FormatPath(path));
  uint64_t parent_inode_id, new_inode_id;
  Inode parent_inode, new_inode;
  int ret = PathLookupToDentry(parent_path, uid, gid, parent_inode_id);
  if (ret != 0) {
    return ret;
  }
  CentralLockInode(parent_inode_id);
  // get parent inode
  ret = GetInodeById(parent_inode_id, parent_inode);
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id);
    return ret;
  }
  if (!IsDir(parent_inode.mode)) {
    CentralUnlockInode(parent_inode_id);
    return -ENOTDIR;
  }
  // check permission
  if (!CheckWritePermission(parent_inode, uid, gid)) {
    CentralUnlockInode(parent_inode_id);
    return -EACCES;
  }
  // check if exists
  ret = GetDentryByParentIdAndName(parent_inode_id, name, new_inode_id);
  if (ret == 0) {
    CentralUnlockInode(parent_inode_id);
    return -EEXIST;
  }
  // create new inode
  new_inode.id = GetNewInodeId();
  new_inode.mode = S_IFREG | mode;
  new_inode.uid = uid;
  new_inode.gid = gid;
  new_inode.size = 0;
  new_inode.atime = new_inode.mtime = new_inode.ctime = time(nullptr);
  // insert new inode
  ret = InsertInode(new_inode.id, new_inode);
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id);
    return ret;
  }
  // insert new dentry
  ret = InsertDentry(parent_inode_id, name, new_inode.id);
  if (ret != 0) {
    DeleteInode(new_inode.id);
    CentralUnlockInode(parent_inode_id);
    return ret;
  }
  // update parent inode
  parent_inode.children++;
  ret = UpdateInode(parent_inode_id, parent_inode);
  if (ret != 0) {
    DeleteInode(new_inode.id);
    DeleteDentry(parent_inode_id, name);
    CentralUnlockInode(parent_inode_id);
    return ret;
  }
  CentralUnlockInode(parent_inode_id);
  return 0;
}

int Engine::Unlink(const std::string& path, uint32_t uid, uint32_t gid) {
  // get parent id
  auto [parent_path, name] = SeparateLastComponent(FormatPath(path));
  uint64_t parent_inode_id, target_inode_id;
  Inode parent_inode, target_inode;
  int ret = PathLookupToDentry(parent_path, uid, gid, parent_inode_id);
  if (ret != 0) {
    return ret;
  }
  // lock parent inode
  CentralLockInode(parent_inode_id);
  // get parent inode
  ret = GetInodeById(parent_inode_id, parent_inode);
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id);
    return ret;
  }
  if (!IsDir(parent_inode.mode)) {
    CentralUnlockInode(parent_inode_id);
    return -ENOTDIR;
  }
  // check permission
  if (!CheckWritePermission(parent_inode, uid, gid)) {
    CentralUnlockInode(parent_inode_id);
    return -EACCES;
  }
  // check if the target dir exists
  ret = GetDentryByParentIdAndName(parent_inode_id, name, target_inode_id);
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id);
    return ret;
  }
  // Lock the target inode
  CentralLockInode(target_inode_id);
  // get target inode
  ret = GetInodeById(target_inode_id, target_inode);
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id);
    CentralUnlockInode(target_inode_id);
    return ret;
  }
  if (IsDir(target_inode.mode)) {
    CentralUnlockInode(parent_inode_id);
    CentralUnlockInode(target_inode_id);
    return -EISDIR;
  }
  // delete target inode
  parent_inode.children--;
  ret = DeleteInode(target_inode_id);
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id);
    CentralUnlockInode(target_inode_id);
    return ret;
  }
  ret = DeleteDentry(parent_inode_id, name); 
  if (ret != 0) {
    InsertInode(target_inode_id, target_inode);
    CentralUnlockInode(parent_inode_id);
    CentralUnlockInode(target_inode_id);
    return ret;
  }
  ret = UpdateInode(parent_inode_id, parent_inode);
  if (ret != 0) {
    InsertInode(target_inode_id, target_inode);
    InsertDentry(parent_inode_id, name, target_inode_id);
    CentralUnlockInode(parent_inode_id);
    CentralUnlockInode(target_inode_id);
  }
  CentralUnlockInode(parent_inode_id);
  CentralUnlockInode(target_inode_id);
  return ret;
}

int Engine::Stat(const std::string& path, uint32_t uid, uint32_t gid, _OUT Inode& inode) {
  uint64_t inode_id;
  int ret = PathLookupToDentry(path, uid, gid, inode_id);
  if (ret != 0) {
    return ret;
  }
  ret = GetInodeById(inode_id, inode);
  return ret;
}



} // namespace fuseefs
