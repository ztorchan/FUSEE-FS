#include <gflags/gflags.h>

#include "fs/central/central.h"

DEFINE_string(erpc_hostname, "", "server erpc hostname");
DEFINE_uint32(erpc_port, 0, "server erpc port");


int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  fuseefs::Central central_server(FLAGS_erpc_hostname, FLAGS_erpc_port);
  central_server.MainLoop();
}