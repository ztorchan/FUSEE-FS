#include <gflags/gflags.h>

#include "src/kv_utils.h"
#include "fs/mds/mds.h"

DEFINE_string(erpc_hostname, "", "server erpc hostname");
DEFINE_uint32(erpc_port, 0, "server erpc port");
DEFINE_string(central_erpc_hostname, "", "central erpc hostname");
DEFINE_uint32(central_erpc_port, 0, "central erpc port");
DEFINE_string(fusee_client_conf, "", "config file path");


int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  GlobalConfig fusee_client_conf;
  load_config(FLAGS_fusee_client_conf.c_str(), &fusee_client_conf);

  fuseefs::MDS mds(FLAGS_erpc_hostname, FLAGS_erpc_port, FLAGS_central_erpc_hostname, FLAGS_central_erpc_port, fusee_client_conf);
  mds.MainLoop();
}