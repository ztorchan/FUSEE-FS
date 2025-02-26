#include "fs/common/erpc.h"
#include "fs/common/type.h"
#include "fs/central/erpc.h"
#include "fs/central/central.h"

namespace fuseefs
{

Central::Central(std::string erpc_hostname, uint16_t erpc_port)
: nexus_(nullptr)
, end_(false) {
  // Initialize the nexus
  nexus_ = new erpc::Nexus(erpc_hostname + ":" + std::to_string(erpc_port));
  nexus_->register_req_func(kLock, LockHandler);
  nexus_->register_req_func(kUnlock, UnlockHandler);
}

Central::~Central() {
  delete nexus_;
}

void Central::MainLoop() {
  CentralServerContext ctx;
  ctx.rpc_id_ = kCentralErpcId;
  ctx.rpc_ = new erpc::Rpc<erpc::CTransport>(nexus_, &ctx, ctx.rpc_id_, SmHandler);
  ctx.owner_ = this;
  while (!end_) {
    ctx.rpc_->run_event_loop(1000);
  }
  delete ctx.rpc_;
}

void Central::LockHandler(erpc::ReqHandle* req_handle, void* _ctx) {
  CentralServerContext* ctx = reinterpret_cast<CentralServerContext*>(_ctx);
  Central* central = reinterpret_cast<Central*>(ctx->owner_);
  // try to lock
  uint64_t* ids = reinterpret_cast<uint64_t*>(req_handle->get_req_msgbuf()->buf_);
  std::unique_lock<std::mutex> lck(central->lck_set_mtx_);
  for (int i = 0; i < kMaxLockInodeNum; i++) {
    if (ids[i] == kInvalidInodeId) {
      break;
    }
    if (central->lock_set_.count(ids[i]) != 0) {
      erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
      erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, sizeof(uint8_t));
      resp.buf_[0] = 1;
      ctx->rpc_->enqueue_response(req_handle, &resp);
      return;
    }
  }
  for (int i = 0; i < kMaxLockInodeNum; i++) {
    if (ids[i] == kInvalidInodeId) {
      break;
    }
    central->lock_set_.insert(ids[i]);
  }
  erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
  erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, sizeof(uint8_t));
  resp.buf_[0] = 0;
  ctx->rpc_->enqueue_response(req_handle, &resp);
}

void Central::UnlockHandler(erpc::ReqHandle* req_handle, void* _ctx) {
  CentralServerContext* ctx = reinterpret_cast<CentralServerContext*>(_ctx);
  Central* central = reinterpret_cast<Central*>(ctx->owner_);
  // unlock
  uint64_t* ids = reinterpret_cast<uint64_t*>(req_handle->get_req_msgbuf()->buf_);
  std::unique_lock<std::mutex> lck(central->lck_set_mtx_);
  for (int i = 0; i < kMaxLockInodeNum; i++) {
    if (ids[i] == kInvalidInodeId) {
      break;
    }
    central->lock_set_.erase(ids[i]);
  }
  erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
  erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, sizeof(uint8_t));
  resp.buf_[0] = 0;
  ctx->rpc_->enqueue_response(req_handle, &resp);
}

void Central::GetInodeIdGroupHandler(erpc::ReqHandle* req_handle, void* _ctx) {
  CentralServerContext* ctx = reinterpret_cast<CentralServerContext*>(_ctx);
  Central* central = reinterpret_cast<Central*>(ctx->owner_);
  // get inode id group
  erpc::MsgBuffer& resp = req_handle->pre_resp_msgbuf_;
  erpc::Rpc<erpc::CTransport>::resize_msg_buffer(&resp, sizeof(uint64_t));
  *(reinterpret_cast<uint64_t*>(resp.buf_)) = central->inode_id_group_seq_.fetch_add(1);
  ctx->rpc_->enqueue_response(req_handle, &resp);
}

} // namespace fuseefs