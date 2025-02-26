#include <gflags/gflags.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

#include "src/server.h"

DEFINE_int32(fusee_pmpool_server_id, 0, "pmpool server id");
DEFINE_string(fusee_pmpool_conf, "", "config file path");

int main(int argc, char ** argv) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  int32_t ret = 0;
  struct GlobalConfig server_conf;
  ret = load_config(FLAGS_fusee_pmpool_conf.c_str(), &server_conf);
  assert(ret == 0);
  server_conf.server_id = FLAGS_fusee_pmpool_server_id;

  printf("===== Starting Server %d =====\n", server_conf.server_id);
  Server * server = new Server(&server_conf);
  ServerMainArgs server_main_args;
  server_main_args.server = server;
  server_main_args.core_id = server_conf.main_core_id;

  pthread_t server_tid;
  pthread_create(&server_tid, NULL, server_main, (void *)&server_main_args);

  printf("press to exit\n");
  // getchar();
  printf("===== Ending Server %d =====\n", server_conf.server_id);
  sleep(100000000ll);

  server->stop();
  return 0;
}