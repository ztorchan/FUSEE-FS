#include <gflags/gflags.h>

#include "fs/engine/engine.h"

DEFINE_string(local_erpc_hostname, "", "local erpc hostname");
DEFINE_uint32(local_erpc_port, 0, "local erpc port");
DEFINE_string(central_erpc_hostname, "", "central erpc hostname");
DEFINE_uint32(central_erpc_port, 0, "central erpc port");
DEFINE_string(fusee_client_conf, "", "fusee client config file");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  GlobalConfig fusee_client_conf;
  int ret = load_config(FLAGS_fusee_client_conf.c_str(), &fusee_client_conf);
  assert(ret == 0);

  fuseefs::Engine engine(fusee_client_conf, FLAGS_central_erpc_hostname, FLAGS_central_erpc_port,
                         FLAGS_local_erpc_hostname, FLAGS_local_erpc_port);
  engine.InitThread(0);

  ret = engine.Mkdir("/test", 0, 0, 0755);
  assert(ret == 0);
  ret = engine.Mkdir("/test/test1", 0, 0, 0755);
  assert(ret == 0);
  ret = engine.Mkdir("/test/test2", 0, 0, 0755);
  assert(ret == 0);
  ret = engine.Mkdir("/test/test3", 0, 0, 0755);
  assert(ret == 0);

  ret = engine.Creat("/test/test1/file1", 0, 0, 0644);
  assert(ret == 0);
  ret = engine.Creat("/test/test1/file2", 0, 0, 0644);
  assert(ret == 0);
  ret = engine.Creat("/test/test1/file3", 0, 0, 0644);
  assert(ret == 0);

  fuseefs::Inode inode;
  ret = engine.Stat("/test/test1/file1", 0, 0, inode);
  assert(ret == 0);
  ret = engine.Stat("/test/test1/file2", 0, 0, inode);
  assert(ret == 0);
  ret = engine.Stat("/test/test1/file3", 0, 0, inode);
  assert(ret == 0);

  ret = engine.Unlink("/test/test1/file1", 0, 0);
  assert(ret == 0);
  ret = engine.Unlink("/test/test1/file2", 0, 0);
  assert(ret == 0);
  ret = engine.Unlink("/test/test1/file3", 0, 0);
  assert(ret == 0);

  ret = engine.Rmdir("/test/test1", 0, 0);
  assert(ret == 0);
  ret = engine.Rmdir("/test/test2", 0, 0);
  assert(ret == 0);
  ret = engine.Rmdir("/test/test3", 0, 0);
  assert(ret == 0);
  ret = engine.Rmdir("/test", 0, 0);
  assert(ret == 0);

  printf("test passed\n");

  return 0;
}