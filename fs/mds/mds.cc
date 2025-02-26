#include "fs/mds/mds.h"
#include "fs/central/erpc.h"
#include "fs/common/erpc.h"
#include "fs/common/thread.h"
#include "fs/common/path.h"
#include "fs/common/meta.h"
#include "fs/fbs_h/mds_request_generated.h"

namespace fuseefs
{

MDS::MDS(std::string erpc_hostname, uint16_t erpc_port,  // erpc server
      std::string central_erpc_hostname, uint16_t central_erpc_port,  // central erpc client
      const GlobalConfig& fusee_client_conf)
  : fusee_client_conf_(fusee_client_conf)
  , central_erpc_hostname_(central_erpc_hostname)
  , central_erpc_port_(central_erpc_port)
  , nexus_(nullptr)
  , end_(false) {
  // Initialize the mds server nexus
  nexus_ = new erpc::Nexus(erpc_hostname + ":" + std::to_string(erpc_port));
  nexus_->register_req_func(kMkdir, MkdirHandler);
  nexus_->register_req_func(kRmdir, RmdirHandler);
  nexus_->register_req_func(kCreat, CreatHandler);
  nexus_->register_req_func(kUnlink, UnlinkHandler);
  nexus_->register_req_func(kStat, StatHandler);
  // nexus_->register_req_func(kRename, RenameHandler);
}

MDS::~MDS() {
  delete nexus_;
}

void MDS::MainLoop() {
  std::vector<uint64_t> core_ids = GetCpuAffinity();
  core_ids = {0};
  for (const uint64_t core_id : core_ids) {
    threads_.emplace_back(std::thread(ThreadLoopOnOneCore, this, core_id));
  }
  for (std::thread& thread : threads_) {
    thread.join();
  }
}

void MDS::ThreadLoopOnOneCore(MDS* mds, uint64_t core_id) {
  printf("[MDS.ThreadLoopOnOneCore] core_id = %lu\n", core_id);
  BindToCore(core_id);
  MDSServerContext ctx;
  ctx.rpc_id_ = core_id;
  ctx.rpc_ = new erpc::Rpc<erpc::CTransport>(mds->nexus_, &ctx, core_id, SmHandler);
  ctx.central_client_session_id_ = ctx.rpc_->create_session(
  mds->central_erpc_hostname_ + ":" + std::to_string(mds->central_erpc_port_), kCentralErpcId);  
  ctx.fusee_client_ = new Client(&mds->fusee_client_conf_);
  ctx.owner_ = mds;
  if (core_id == 0) {
    printf("[MDS.ThreadLoopOnOneCore] create root inode\n");
    // create root inode
    Inode root_inode;
    root_inode.id = kRootInodeId;
    root_inode.mode = S_IFDIR | 0755;
    root_inode.uid = 0;
    root_inode.gid = 0;
    root_inode.size = 0;
    root_inode.atime = root_inode.mtime = root_inode.ctime = time(nullptr);
    mds->InsertInode(kRootInodeId, root_inode, &ctx);
    printf("[MDS.ThreadLoopOnOneCore] create root inode done\n");
  }
  while (!mds->end_) {
    ctx.rpc_->run_event_loop(1000);
  }
  delete ctx.fusee_client_;
  delete ctx.rpc_;
}

uint64_t MDS::GetNewInodeId(MDSServerContext* ctx) {
  MDS* mds = reinterpret_cast<MDS*>(ctx->owner_);
  uint64_t new_inode_seq = mds->inode_id_seq_in_group_.fetch_add(1);
  while (new_inode_seq >= kInodeIdGroupSize) {
    // get new group
    std::unique_lock<std::mutex> lck(mds->inode_id_group_mtx_);
    if ((new_inode_seq = mds->inode_id_seq_in_group_.fetch_add(1)) >= kInodeIdGroupSize) {
      // double check
      break;
    }

    erpc::MsgBuffer req = ctx->rpc_->alloc_msg_buffer(16);
    erpc::MsgBuffer rsp = ctx->rpc_->alloc_msg_buffer(sizeof(uint64_t));
    ContContextT cctx = {0, reinterpret_cast<char*>(rsp.buf_)};
    ctx->rpc_->enqueue_request(ctx->central_client_session_id_, kGetInodeIdGroup, &req, &rsp, ContFunc, &cctx);
    while (cctx.finished_ == 0) {
      ctx->rpc_->run_event_loop(1000);
    }
    inode_id_group_.store(*reinterpret_cast<uint64_t*>(rsp.buf_));
    inode_id_seq_in_group_.store(0);
    ctx->rpc_->free_msg_buffer(req);
    ctx->rpc_->free_msg_buffer(rsp);

    new_inode_seq = inode_id_seq_in_group_.fetch_add(1);
  }
  return inode_id_group_.load() * kInodeIdGroupSize + new_inode_seq + 1;
}

int MDS::GetInodeById(const uint64_t id, MDSServerContext* ctx, _OUT Inode& inode) {
  std::string key = GetInodeKey(id);
  KVInfo kv_info;
  kv_info.l_addr = ctx->fusee_client_->get_input_buf();
  kv_info.lkey = ctx->fusee_client_->get_input_buf_lkey();
  kv_info.key_len = key.size();
  memcpy(kv_info.l_addr, key.c_str(), key.size());
  void* value_addr = ctx->fusee_client_->kv_search(&kv_info);
  if (value_addr == nullptr) {
    return -ENOENT;
  }
  inode = *reinterpret_cast<Inode*>(value_addr);
  return 0;
}

int MDS::GetDentryByParentIdAndName(const uint64_t pid, const std::string& name, 
                                    MDSServerContext* ctx, _OUT uint64_t& inode_id) {
  std::string key = GetDentryKey(pid, name);
  KVInfo kv_info;
  kv_info.l_addr = ctx->fusee_client_->get_input_buf();
  kv_info.lkey = ctx->fusee_client_->get_input_buf_lkey();
  kv_info.key_len = key.size();
  memcpy(kv_info.l_addr, key.c_str(), key.size());
  void* value_addr = ctx->fusee_client_->kv_search(&kv_info);
  if (value_addr == nullptr) {
    return -ENOENT;
  }
  inode_id = *reinterpret_cast<uint64_t*>(value_addr);
  return 0;
}

int MDS::PathLookupToDentry(const std::string& path, uint32_t uid, uint32_t gid, MDSServerContext* ctx,
                            _OUT uint64_t& inode_id) {
  uint64_t cur_inode_id, cur_parent_inode_id;
  Inode inode;
  std::vector<std::string> components = SplitPath(FormatPath(path));
  if (components.empty()) {
    inode_id = kRootInodeId;
    return 0;
  }
  // get root inode
  cur_parent_inode_id = kRootInodeId;
  int ret = GetInodeById(cur_parent_inode_id, ctx, inode);
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
    ret = GetDentryByParentIdAndName(cur_parent_inode_id, name, ctx, cur_inode_id);
    if (ret != 0) {
      return ret;
    }
    if (i == components.size() - 1) {
      break;
    }
    // get inode and check
    ret = GetInodeById(cur_inode_id, ctx, inode);
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

void MDS::CentralLockInode(uint64_t inode_id, MDSServerContext* ctx) {
  bool locked = false;
  erpc::MsgBuffer req = ctx->rpc_->alloc_msg_buffer(sizeof(uint64_t) * kMaxLockInodeNum);
  erpc::MsgBuffer rsp = ctx->rpc_->alloc_msg_buffer(1);
  reinterpret_cast<uint64_t*>(req.buf_)[0] = inode_id;
  while (!locked) {
    ContContextT cctx = {0, reinterpret_cast<char*>(rsp.buf_)};
    ctx->rpc_->enqueue_request(ctx->central_client_session_id_, kLock, &req, &rsp, ContFunc, &cctx);
    while (cctx.finished_ == 0) {
      ctx->rpc_->run_event_loop(1000);
    }
    if (rsp.buf_[0] == 0) {
      locked = true;
    }
  }
  ctx->rpc_->free_msg_buffer(req);
  ctx->rpc_->free_msg_buffer(rsp);
}

void MDS::CentralUnlockInode(uint64_t inode_id, MDSServerContext* ctx) {
  bool locked = false;
  erpc::MsgBuffer req = ctx->rpc_->alloc_msg_buffer(sizeof(uint64_t) * kMaxLockInodeNum);
  erpc::MsgBuffer rsp = ctx->rpc_->alloc_msg_buffer(1);
  reinterpret_cast<uint64_t*>(req.buf_)[0] = inode_id;
  ContContextT cctx = {0, reinterpret_cast<char*>(rsp.buf_)};
  ctx->rpc_->enqueue_request(ctx->central_client_session_id_, kUnlock, &req, &rsp, ContFunc, &cctx);
  while (cctx.finished_ == 0) {
    ctx->rpc_->run_event_loop(1000);
  }
  ctx->rpc_->free_msg_buffer(req);
  ctx->rpc_->free_msg_buffer(rsp);
}

int MDS::InsertDentry(uint64_t parent_id, const std::string& name, uint64_t inode_id, MDSServerContext* ctx) {
  std::string key = GetDentryKey(parent_id, name);
  KVInfo kv_info;
  kv_info.l_addr = ctx->fusee_client_->get_input_buf();
  kv_info.lkey = ctx->fusee_client_->get_input_buf_lkey();
  kv_info.key_len = key.size();
  memcpy(kv_info.l_addr, key.c_str(), key.size());
  kv_info.value_len = sizeof(uint64_t);
  memcpy(kv_info.l_addr + key.size(), &inode_id, sizeof(uint64_t));
  int ret = ctx->fusee_client_->kv_insert(&kv_info);
  return ret;
}

int MDS::DeleteDentry(uint64_t parent_id, const std::string& name, MDSServerContext* ctx) {
  std::string key = GetDentryKey(parent_id, name);
  KVInfo kv_info;
  kv_info.l_addr = ctx->fusee_client_->get_input_buf();
  kv_info.lkey = ctx->fusee_client_->get_input_buf_lkey();
  kv_info.key_len = key.size();
  memcpy(kv_info.l_addr, key.c_str(), key.size());
  int ret = ctx->fusee_client_->kv_delete(&kv_info);
  return ret;
}

int MDS::UpdateDentry(uint64_t parent_id, const std::string& name, uint64_t inode_id, MDSServerContext* ctx) {
  std::string key = GetDentryKey(parent_id, name);
  KVInfo kv_info;
  kv_info.l_addr = ctx->fusee_client_->get_input_buf();
  kv_info.lkey = ctx->fusee_client_->get_input_buf_lkey();
  kv_info.key_len = key.size();
  memcpy(kv_info.l_addr, key.c_str(), key.size());
  kv_info.value_len = sizeof(uint64_t);
  memcpy(kv_info.l_addr + key.size(), &inode_id, sizeof(uint64_t));
  int ret = ctx->fusee_client_->kv_update(&kv_info);
  return ret;
}

int MDS::InsertInode(uint64_t id, const Inode& inode, MDSServerContext* ctx) {
  std::string key = GetInodeKey(id);
  KVInfo kv_info;
  kv_info.l_addr = ctx->fusee_client_->get_input_buf();
  kv_info.lkey = ctx->fusee_client_->get_input_buf_lkey();
  kv_info.key_len = key.size();
  memcpy(kv_info.l_addr, key.c_str(), key.size());
  kv_info.value_len = sizeof(Inode);
  memcpy(kv_info.l_addr + key.size(), &inode, sizeof(Inode));
  int ret = ctx->fusee_client_->kv_insert(&kv_info);
  return ret;
}

int MDS::DeleteInode(uint64_t id, MDSServerContext* ctx) {
  std::string key = GetInodeKey(id);
  KVInfo kv_info;
  kv_info.l_addr = ctx->fusee_client_->get_input_buf();
  kv_info.lkey = ctx->fusee_client_->get_input_buf_lkey();
  kv_info.key_len = key.size();
  memcpy(kv_info.l_addr, key.c_str(), key.size());
  int ret = ctx->fusee_client_->kv_delete(&kv_info);
  return ret;
}

int MDS::UpdateInode(uint64_t id, const Inode& inode, MDSServerContext* ctx) {
  std::string key = GetInodeKey(id);
  KVInfo kv_info;
  kv_info.l_addr = ctx->fusee_client_->get_input_buf();
  kv_info.lkey = ctx->fusee_client_->get_input_buf_lkey();
  kv_info.key_len = key.size();
  memcpy(kv_info.l_addr, key.c_str(), key.size());
  kv_info.value_len = sizeof(Inode);
  memcpy(kv_info.l_addr + key.size(), &inode, sizeof(Inode));
  int ret = ctx->fusee_client_->kv_update(&kv_info);
  return ret;
}

void MDS::StatHandler(erpc::ReqHandle* req_handle, void* _ctx) {
  // parse request
  MDSServerContext* ctx = static_cast<MDSServerContext*>(_ctx);
  MDS* mds = static_cast<MDS*>(ctx->owner_);
  const MDRequest* req = GetMDRequest(req_handle->get_req_msgbuf()->buf_);
  uint32_t uid = req->uid();
  uint32_t gid = req->gid();
  std::string path = req->path()->str();
  printf("[MDS.StatHandler] stat %s\n", path.c_str());
  // lookup inode
  Inode inode;
  uint64_t inode_id;
  int ret = PathLookupToDentry(path, uid, gid, ctx, inode_id);
  if (ret == 0) {
    ret = GetInodeById(inode_id, ctx, inode);
  }
  // response
  erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
  erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1 + sizeof(Inode));
  resp.buf_[0] = ret;
  memcpy(resp.buf_ + 1, &inode, sizeof(Inode));
  ctx->rpc_->enqueue_response(req_handle, &resp);
}

void MDS::MkdirHandler(erpc::ReqHandle* req_handle, void* _ctx) {
  // parse request
  MDSServerContext* ctx = static_cast<MDSServerContext*>(_ctx);
  MDS* mds = static_cast<MDS*>(ctx->owner_);
  const MDRequest* req = GetMDRequest(req_handle->get_req_msgbuf()->buf_);
  uint32_t uid = req->uid();
  uint32_t gid = req->gid();
  uint32_t mode = req->mode();
  std::string path = req->path()->str();
  printf("[MDS.MkdirHandler] mkdir %s\n", path.c_str());
  // get parent id
  auto [parent_path, name] = SeparateLastComponent(FormatPath(path));
  uint64_t parent_inode_id, new_inode_id;
  Inode parent_inode, new_inode;
  int ret = PathLookupToDentry(parent_path, uid, gid, ctx, parent_inode_id);
  if (ret != 0) {
    erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
    erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
    resp.buf_[0] = ret;
    ctx->rpc_->enqueue_response(req_handle, &resp);
    return;
  }
  // lock parent inode
  CentralLockInode(parent_inode_id, ctx);
  // get parent inode
  ret = GetInodeById(parent_inode_id, ctx, parent_inode);
  if (ret == 0) {
    if (!IsDir(parent_inode.mode)) {
      ret = -ENOTDIR;
    } else if (!CheckWritePermission(parent_inode, uid, gid)) {
      ret = -EACCES;
    }
  }
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id, ctx);
    erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
    erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
    resp.buf_[0] = ret;
    ctx->rpc_->enqueue_response(req_handle, &resp);
    return;
  }
  // check if the target inode exists
  ret = GetDentryByParentIdAndName(parent_inode_id, name, ctx, new_inode_id);
  if (ret != -ENOENT) {
    CentralUnlockInode(parent_inode_id, ctx);
    erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
    erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
    resp.buf_[0] = -EEXIST;
    ctx->rpc_->enqueue_response(req_handle, &resp);
    return;
  }
  // insert new inode
  new_inode.id = mds->GetNewInodeId(ctx);
  new_inode.size = 0;
  new_inode.atime = new_inode.mtime = new_inode.ctime = time(nullptr);
  new_inode.children = 0;
  new_inode.mode = S_IFDIR | mode;
  new_inode.uid = uid;
  new_inode.gid = gid;
  parent_inode.children++;
  InsertInode(new_inode.id, new_inode, ctx);
  InsertDentry(parent_inode_id, name, new_inode.id, ctx);
  UpdateInode(parent_inode_id, parent_inode, ctx);
  // unlock parent inode
  CentralUnlockInode(parent_inode_id, ctx);
  // response
  erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
  erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
  resp.buf_[0] = 0;
  ctx->rpc_->enqueue_response(req_handle, &resp);
  return ;
}

void MDS::RmdirHandler(erpc::ReqHandle* req_handle, void* _ctx) {
  // parse request
  MDSServerContext* ctx = static_cast<MDSServerContext*>(_ctx);
  MDS* mds = static_cast<MDS*>(ctx->owner_);
  const MDRequest* req = GetMDRequest(req_handle->get_req_msgbuf()->buf_);
  uint32_t uid = req->uid();
  uint32_t gid = req->gid();
  std::string path = req->path()->str();
  printf("[MDS.RmdirHandler] rmdir %s\n", path.c_str());
  // get parent id
  auto [parent_path, name] = SeparateLastComponent(FormatPath(path));
  uint64_t parent_inode_id, target_inode_id;
  Inode parent_inode, target_inode;
  int ret = PathLookupToDentry(parent_path, uid, gid, ctx, parent_inode_id);
  if (ret != 0) {
    erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
    erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
    resp.buf_[0] = ret;
    ctx->rpc_->enqueue_response(req_handle, &resp);
    return;
  }
  // lock parent inode
  CentralLockInode(parent_inode_id, ctx);
  // get parent inode
  ret = GetInodeById(parent_inode_id, ctx, parent_inode);
  if (ret == 0) {
    if (!IsDir(parent_inode.mode)) {
      ret = -ENOTDIR;
    } else if (!CheckWritePermission(parent_inode, uid, gid)) {
      ret = -EACCES;
    }
  }
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id, ctx);
    erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
    erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
    resp.buf_[0] = ret;
    ctx->rpc_->enqueue_response(req_handle, &resp);
    return;
  }
  // check if the target dir exists
  ret = GetDentryByParentIdAndName(parent_inode_id, name, ctx, target_inode_id);
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id, ctx);
    erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
    erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
    resp.buf_[0] = ret;
    ctx->rpc_->enqueue_response(req_handle, &resp);
    return;
  }
  // Lock the target inode
  CentralLockInode(target_inode_id, ctx);
  // get target inode
  ret = GetInodeById(target_inode_id, ctx, target_inode);
  if (ret == 0) {
    if (!IsDir(target_inode.mode)) {
      ret = -ENOTDIR;
    } else if (target_inode.children > 0) {
      ret = -ENOTEMPTY;
    }
  }
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id, ctx);
    CentralUnlockInode(target_inode_id, ctx);
    erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
    erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
    resp.buf_[0] = ret;
    ctx->rpc_->enqueue_response(req_handle, &resp);
    return;
  }
  // delete target inode
  parent_inode.children--;
  DeleteInode(target_inode_id, ctx);
  DeleteDentry(parent_inode_id, name, ctx);
  UpdateInode(parent_inode_id, parent_inode, ctx);
  // unlock 
  CentralUnlockInode(target_inode_id, ctx);
  CentralUnlockInode(parent_inode_id, ctx);
  // response
  erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
  erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
  resp.buf_[0] = 0;
  ctx->rpc_->enqueue_response(req_handle, &resp);
}

void MDS::CreatHandler(erpc::ReqHandle* req_handle, void* _ctx) {
  // parse request
  MDSServerContext* ctx = static_cast<MDSServerContext*>(_ctx);
  MDS* mds = static_cast<MDS*>(ctx->owner_);
  const MDRequest* req = GetMDRequest(req_handle->get_req_msgbuf()->buf_);
  uint32_t uid = req->uid();
  uint32_t gid = req->gid();
  uint32_t mode = req->mode();
  std::string path = req->path()->str();
  printf("[MDS.CreatHandler] creat %s\n", path.c_str());
  // get parent id
  auto [parent_path, name] = SeparateLastComponent(FormatPath(path));
  uint64_t parent_inode_id, new_inode_id;
  Inode parent_inode, new_inode;
  int ret = PathLookupToDentry(parent_path, uid, gid, ctx, parent_inode_id);
  if (ret != 0) {
    erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
    erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
    resp.buf_[0] = ret;
    ctx->rpc_->enqueue_response(req_handle, &resp);
    return;
  }
  // lock parent inode
  CentralLockInode(parent_inode_id, ctx);
  // get parent inode
  ret = GetInodeById(parent_inode_id, ctx, parent_inode);
  if (ret == 0) {
    if (!IsDir(parent_inode.mode)) {
      ret = -ENOTDIR;
    } else if (!CheckWritePermission(parent_inode, uid, gid)) {
      ret = -EACCES;
    }
  }
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id, ctx);
    erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
    erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
    resp.buf_[0] = ret;
    ctx->rpc_->enqueue_response(req_handle, &resp);
    return;
  }
  // check if the target inode exists
  ret = GetDentryByParentIdAndName(parent_inode_id, name, ctx, new_inode_id);
  if (ret != -ENOENT) {
    CentralUnlockInode(parent_inode_id, ctx);
    erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
    erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
    resp.buf_[0] = -EEXIST;
    ctx->rpc_->enqueue_response(req_handle, &resp);
    return;
  }
  // insert new inode
  new_inode.id = mds->GetNewInodeId(ctx);
  new_inode.size = 0;
  new_inode.atime = new_inode.mtime = new_inode.ctime = time(nullptr);
  new_inode.children = 0;
  new_inode.mode = S_IFREG | mode;
  new_inode.uid = uid;
  new_inode.gid = gid;
  parent_inode.children++;
  InsertInode(new_inode.id, new_inode, ctx);
  InsertDentry(parent_inode_id, name, new_inode.id, ctx);
  UpdateInode(parent_inode_id, parent_inode, ctx);
  // unlock parent inode
  CentralUnlockInode(parent_inode_id, ctx);
  // response
  erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
  erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
  resp.buf_[0] = 0;
  ctx->rpc_->enqueue_response(req_handle, &resp);
  return ;
}

void MDS::UnlinkHandler(erpc::ReqHandle* req_handle, void* _ctx) {
  // parse request
  MDSServerContext* ctx = static_cast<MDSServerContext*>(_ctx);
  MDS* mds = static_cast<MDS*>(ctx->owner_);
  const MDRequest* req = GetMDRequest(req_handle->get_req_msgbuf()->buf_);
  uint32_t uid = req->uid();
  uint32_t gid = req->gid();
  std::string path = req->path()->str();
  printf("[MDS.UnlinkHandler] unlink %s\n", path.c_str());
  // get parent id
  auto [parent_path, name] = SeparateLastComponent(FormatPath(path));
  uint64_t parent_inode_id, target_inode_id;
  Inode parent_inode, target_inode;
  int ret = PathLookupToDentry(parent_path, uid, gid, ctx, parent_inode_id);
  if (ret != 0) {
    erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
    erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
    resp.buf_[0] = ret;
    ctx->rpc_->enqueue_response(req_handle, &resp);
    return;
  }
  // lock parent inode
  CentralLockInode(parent_inode_id, ctx);
  // get parent inode
  ret = GetInodeById(parent_inode_id, ctx, parent_inode);
  if (ret == 0) {
    if (!IsDir(parent_inode.mode)) {
      ret = -ENOTDIR;
    } else if (!CheckWritePermission(parent_inode, uid, gid)) {
      ret = -EACCES;
    }
  }
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id, ctx);
    erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
    erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
    resp.buf_[0] = ret;
    ctx->rpc_->enqueue_response(req_handle, &resp);
    return;
  }
  // check if the target dir exists
  ret = GetDentryByParentIdAndName(parent_inode_id, name, ctx, target_inode_id);
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id, ctx);
    erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
    erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
    resp.buf_[0] = ret;
    ctx->rpc_->enqueue_response(req_handle, &resp);
    return;
  }
  // Lock the target inode
  CentralLockInode(target_inode_id, ctx);
  // get target inode
  ret = GetInodeById(target_inode_id, ctx, target_inode);
  if (ret == 0) {
    if (IsDir(target_inode.mode)) {
      ret = -EISDIR;
    }
  }
  if (ret != 0) {
    CentralUnlockInode(parent_inode_id, ctx);
    CentralUnlockInode(target_inode_id, ctx);
    erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
    erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
    resp.buf_[0] = ret;
    ctx->rpc_->enqueue_response(req_handle, &resp);
    return;
  }
  // delete target inode
  parent_inode.children--;
  DeleteInode(target_inode_id, ctx);
  DeleteDentry(parent_inode_id, name, ctx);
  UpdateInode(parent_inode_id, parent_inode, ctx);
  // unlock
  CentralUnlockInode(target_inode_id, ctx);
  CentralUnlockInode(parent_inode_id, ctx);
  // response
  erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
  erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, 1);
  resp.buf_[0] = 0;
  ctx->rpc_->enqueue_response(req_handle, &resp);
}

// void MDS::RenameHandler(erpc::ReqHandle* req_handle, void* _ctx) {
  
// }

} // namespace fuseefs
