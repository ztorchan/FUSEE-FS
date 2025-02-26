#ifdef ASSERT
#include <cassert>
#endif

#include <cstdlib>
#include <string>
#include <memory>
#include <fstream>
#include <sstream>

#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <flatbuffers/flatbuffer_builder.h>

#include "fs/client/client.h"
#include "fs/client/posix_wrapper.h"
#include "fs/common/erpc.h"
#include "fs/common/type.h"
#include "fs/mds/erpc.h"
#include "fs/fbs_h/mds_request_generated.h"

#define CLINET_CONFIG_ENV "FUSEEFS_CLIENT_CONFIG"

std::unique_ptr<fuseefs::ClientContext> global_client_context_;

void ClientSessionHandler(int session_num, erpc::SmEventType sm_event_type,
                          erpc::SmErrType sm_err_type, void *_c) {
#ifdef ASSERT
  assert(sm_err_type == erpc::SmErrType::kNoError);
#endif
  if (sm_event_type == erpc::SmEventType::kConnected) {
    global_client_context_->__ready_sessions_.fetch_add(1);
  } else if (sm_event_type == erpc::SmEventType::kConnected) {
    global_client_context_->__ready_sessions_.fetch_sub(1);
  } else {
#ifdef ASSERT
    assert(false);
#endif
  }
}

void InitWrapper() {
  srand(time(NULL));

  // init global client context
  global_client_context_ = std::make_unique<fuseefs::ClientContext>();
  global_client_context_->uid_ = getuid();
  global_client_context_->gid_ = getgid();

  // load config
  char* client_config_path = std::getenv(CLINET_CONFIG_ENV);
  if (client_config_path == nullptr) {
    return;
  }
  std::ifstream ifs(client_config_path);
  std::stringstream buf;
  buf << ifs.rdbuf();
  std::string config_str = buf.str();
  ifs.close();
  rapidjson::Document doc;
  doc.Parse(config_str.c_str());

  // create rpc
  const char *env_node_rank = std::getenv("OMPI_COMM_WORLD_NODE_RANK");
  int node_rank = (env_node_rank != nullptr) ? std::atoi(env_node_rank) : 0;
  global_client_context_->client_erpc_hostname_ = doc["self_host"].GetString();
  global_client_context_->client_erpc_ctl_base_port_ = doc["self_base_port"].GetUint();
  global_client_context_->nexus_ = new erpc::Nexus(global_client_context_->client_erpc_hostname_ + ":" + std::to_string(global_client_context_->client_erpc_ctl_base_port_ + node_rank));
  global_client_context_->rpc_ = new erpc::Rpc<erpc::CTransport>(global_client_context_->nexus_, nullptr, 0, ClientSessionHandler);

  // create session
  uint64_t total_sessions = 0;
  const auto& mds_list = doc["mds"].GetArray();
  for (size_t i = 0; i < mds_list.Size(); i++) {
    std::string mds_erpc_hostname = mds_list[i]["host"].GetString();
    uint16_t mds_erpc_ctl_port = mds_list[i]["port"].GetUint();
    int mds_threads_num = mds_list[i]["threads_num"].GetInt();
    
    std::vector<int> cur_mds_sessions;
    std::string mds_uri = mds_erpc_hostname + ":" + std::to_string(mds_erpc_ctl_port);
    for (int t = 0; t < mds_threads_num; t++) {
      int session_num = global_client_context_->rpc_->create_session(mds_uri, t);
      cur_mds_sessions.push_back(session_num);
    }
    global_client_context_->mds_sessions_.push_back(cur_mds_sessions);
    total_sessions += mds_threads_num;
  }

  // wait for all sessions to be ready
  while (global_client_context_->__ready_sessions_.load() != total_sessions) {
    global_client_context_->rpc_->run_event_loop_once();
  }
}

void DestroyWrapper() {
  delete global_client_context_->rpc_;
  delete global_client_context_->nexus_;
  global_client_context_.reset();
}

int get_rand_session() {
  uint64_t global_thread_id = rand() % global_client_context_->__ready_sessions_.load();
  for (int i = 0; i < global_client_context_->mds_sessions_.size(); i++) {
    if (global_client_context_->mds_sessions_[i].size() > global_thread_id) {
      return global_client_context_->mds_sessions_[i][global_thread_id];
    } else {
      global_thread_id -= global_client_context_->mds_sessions_[i].size();
    }
  }
}

int fuseefs_stat(const char* path, struct stat* st) {
  // build request
  flatbuffers::FlatBufferBuilder fbb(1024);
  auto stat_req = fuseefs::CreateMDRequest(fbb, 
                                         fuseefs::MDOpType::MDOpType_Stat, 
                                         global_client_context_->uid_, global_client_context_->gid_, 
                                         fbb.CreateString(path), 0, 
                                         0);
  fbb.Finish(stat_req);
  auto stat_req_buf = fbb.GetBufferPointer();
  auto stat_req_size = fbb.GetSize();

  // send request
  auto req = global_client_context_->rpc_->alloc_msg_buffer_or_die(stat_req_size);
  auto resp = global_client_context_->rpc_->alloc_msg_buffer_or_die(1 + sizeof(fuseefs::Inode));
  memcpy(req.buf_, stat_req_buf, stat_req_size);
  fuseefs::ContContextT cc = {0, reinterpret_cast<char*>(resp.buf_)};
  global_client_context_->rpc_->enqueue_request(get_rand_session(), fuseefs::kStat,
                                                &req, &resp, fuseefs::ContFunc, &cc);
  while (!cc.finished_) {
    global_client_context_->rpc_->run_event_loop_once();
  }

  // return 
  int ret = static_cast<char>(resp.buf_[0]);
  if (ret == 0) {
    fuseefs::Inode* inode = reinterpret_cast<fuseefs::Inode*>(resp.buf_ + 1);
    st->st_ino = inode->id;
    st->st_size = inode->size;
    st->st_atime = inode->atime;
    st->st_mtime = inode->mtime;
    st->st_ctime = inode->ctime;
    st->st_mode = inode->mode;
    st->st_uid = inode->uid;
    st->st_gid = inode->gid;
  }
  global_client_context_->rpc_->free_msg_buffer(req);
  global_client_context_->rpc_->free_msg_buffer(resp);
  return ret;
}

int fuseefs_mkdir(const char *path, mode_t mode) {
  // build request
  flatbuffers::FlatBufferBuilder fbb(1024);
  auto mkdir_req = fuseefs::CreateMDRequest(fbb, 
                                          fuseefs::MDOpType::MDOpType_Mkdir, 
                                          global_client_context_->uid_, global_client_context_->gid_, 
                                          fbb.CreateString(path), 0, 
                                          mode);
  fbb.Finish(mkdir_req);
  auto mkdir_req_buf = fbb.GetBufferPointer();
  auto mkdir_req_size = fbb.GetSize();

  // send request
  auto req = global_client_context_->rpc_->alloc_msg_buffer_or_die(mkdir_req_size);
  auto resp = global_client_context_->rpc_->alloc_msg_buffer_or_die(1);
  memcpy(req.buf_, mkdir_req_buf, mkdir_req_size);
  fuseefs::ContContextT cc = {0, reinterpret_cast<char*>(resp.buf_)};
  global_client_context_->rpc_->enqueue_request(get_rand_session(), fuseefs::kMkdir,
                                                &req, &resp, fuseefs::ContFunc, &cc);
  while (!cc.finished_) {
    global_client_context_->rpc_->run_event_loop_once();
  }

  // return
  int ret = static_cast<char>(resp.buf_[0]);
  global_client_context_->rpc_->free_msg_buffer(req);
  global_client_context_->rpc_->free_msg_buffer(resp);
  return ret;
}

int fuseefs_rmdir(const char *path) {
  // build request
  flatbuffers::FlatBufferBuilder fbb(1024);
  auto rmdir_req = fuseefs::CreateMDRequest(fbb, 
                                          fuseefs::MDOpType::MDOpType_Rmdir, 
                                          global_client_context_->uid_, global_client_context_->gid_, 
                                          fbb.CreateString(path), 0, 
                                          0);
  fbb.Finish(rmdir_req);
  auto rmdir_req_buf = fbb.GetBufferPointer();
  auto rmdir_req_size = fbb.GetSize();

  // send request
  auto req = global_client_context_->rpc_->alloc_msg_buffer_or_die(rmdir_req_size);
  auto resp = global_client_context_->rpc_->alloc_msg_buffer_or_die(1);
  memcpy(req.buf_, rmdir_req_buf, rmdir_req_size);
  fuseefs::ContContextT cc = {0, reinterpret_cast<char*>(resp.buf_)};
  global_client_context_->rpc_->enqueue_request(get_rand_session(), fuseefs::kRmdir,
                                                &req, &resp, fuseefs::ContFunc, &cc);
  while (!cc.finished_) {
    global_client_context_->rpc_->run_event_loop_once();
  }

  // return
  int ret = static_cast<char>(resp.buf_[0]);
  global_client_context_->rpc_->free_msg_buffer(req);
  global_client_context_->rpc_->free_msg_buffer(resp);
  return ret;
}

int fuseefs_creat(const char *path, mode_t mode) {
  // build request
  flatbuffers::FlatBufferBuilder fbb(1024);
  auto creat_req = fuseefs::CreateMDRequest(fbb, 
                                          fuseefs::MDOpType::MDOpType_Creat, 
                                          global_client_context_->uid_, global_client_context_->gid_, 
                                          fbb.CreateString(path), 0, 
                                          mode);
  fbb.Finish(creat_req);
  auto creat_req_buf = fbb.GetBufferPointer();
  auto creat_req_size = fbb.GetSize();

  // send request
  auto req = global_client_context_->rpc_->alloc_msg_buffer_or_die(creat_req_size);
  auto resp = global_client_context_->rpc_->alloc_msg_buffer_or_die(1);
  memcpy(req.buf_, creat_req_buf, creat_req_size);
  fuseefs::ContContextT cc = {0, reinterpret_cast<char*>(resp.buf_)};
  global_client_context_->rpc_->enqueue_request(get_rand_session(), fuseefs::kCreat,
                                                &req, &resp, fuseefs::ContFunc, &cc);
  while (!cc.finished_) {
    global_client_context_->rpc_->run_event_loop_once();
  }

  // return
  int ret = static_cast<char>(resp.buf_[0]);
  global_client_context_->rpc_->free_msg_buffer(req);
  global_client_context_->rpc_->free_msg_buffer(resp);
  return ret;
}

int fuseefs_unlink(const char *path) {
  // build request
  flatbuffers::FlatBufferBuilder fbb(1024);
  auto remove_req = fuseefs::CreateMDRequest(fbb, 
                                           fuseefs::MDOpType::MDOpType_Unlink, 
                                           global_client_context_->uid_, global_client_context_->gid_, 
                                           fbb.CreateString(path), 0, 
                                           0);
  fbb.Finish(remove_req);
  auto remove_req_buf = fbb.GetBufferPointer();
  auto remove_req_size = fbb.GetSize();

  // send request
  auto req = global_client_context_->rpc_->alloc_msg_buffer_or_die(remove_req_size);
  auto resp = global_client_context_->rpc_->alloc_msg_buffer_or_die(1);
  memcpy(req.buf_, remove_req_buf, remove_req_size);
  fuseefs::ContContextT cc = {0, reinterpret_cast<char*>(resp.buf_)};
  global_client_context_->rpc_->enqueue_request(get_rand_session(), fuseefs::kUnlink,
                                                &req, &resp, fuseefs::ContFunc, &cc);
  while (!cc.finished_) {
    global_client_context_->rpc_->run_event_loop_once();
  }

  // return
  int ret = static_cast<char>(resp.buf_[0]);
  global_client_context_->rpc_->free_msg_buffer(req);
  global_client_context_->rpc_->free_msg_buffer(resp);
  return ret;
}
