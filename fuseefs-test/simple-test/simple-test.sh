#!/bin/bash

set -ex

TEST_DIR=$(dirname "$(realpath "$0")")
BIN_DIR=${TEST_DIR}/../../build/bin
LIB_DIR=${TEST_DIR}/../../build/lib

stop_all() {
  kill $mds_pid $pmpool_pid $central_pid
}

${BIN_DIR}/fuseefs_pmpool_server --fusee_pmpool_server_id=0 --fusee_pmpool_conf=${TEST_DIR}/pmpool.json &
pmpool_pid=$!

sleep 3

${BIN_DIR}/fuseefs_central_server --erpc_hostname=127.0.0.1 --erpc_port=31850 &
central_pid=$!

sleep 3

${BIN_DIR}/fuseefs_mds_server --erpc_hostname=127.0.0.1 --erpc_port=31851 --central_erpc_hostname=127.0.0.1 --central_erpc_port=31850 --fusee_client_conf=${TEST_DIR}/mds.json &
mds_pid=$!

trap stop_all EXIT

sleep 100000000

# FUSEEFS_CLIENT_CONFIG=${TEST_DIR}/client.json LD_PRELOAD=${LIB_DIR}/libfuseefs_hook.so ${BIN_DIR}/fuseefs_simple_test