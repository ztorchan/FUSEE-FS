#include <iostream>
#include <chrono>
#include <atomic>
#include <gflags/gflags.h>
#include <algorithm>

#include <gflags/gflags.h>

#include "fs/engine/engine.h"

DEFINE_string(local_erpc_hostname, "", "local erpc hostname");
DEFINE_uint32(local_erpc_port, 0, "local erpc port");
DEFINE_string(central_erpc_hostname, "", "central erpc hostname");
DEFINE_uint32(central_erpc_port, 0, "central erpc port");
DEFINE_string(fusee_client_conf, "", "fusee client config file");
DEFINE_uint64(thread, 1, "thread num");

#define STATISTIC_PRINT_INTERVAL 10000

#define NODE_ID 1

std::mutex printf_mtx;

struct ThreadLocalStatistic {
  uint64_t thread_id;

  std::atomic_uint64_t mkdir_cnt;
  std::atomic_uint64_t rmdir_cnt;
  std::atomic_uint64_t creat_cnt;
  std::atomic_uint64_t unlink_cnt;
  std::atomic_uint64_t stat_cnt;

  std::atomic_uint64_t mkdir_fail_cnt;
  std::atomic_uint64_t rmdir_fail_cnt;
  std::atomic_uint64_t creat_fail_cnt;
  std::atomic_uint64_t unlink_fail_cnt;
  std::atomic_uint64_t stat_fail_cnt;

  std::atomic_uint64_t mkdir_time;
  std::atomic_uint64_t rmdir_time;
  std::atomic_uint64_t creat_time;
  std::atomic_uint64_t unlink_time;
  std::atomic_uint64_t stat_time;

  void* ext;
};

struct GlobalStatistic {
  std::atomic_uint64_t total_cnt;
  std::atomic_uint64_t running_thread;
  ThreadLocalStatistic thread_statistic[256];
};

GlobalStatistic global_statistic;

void init_statistic() {
  global_statistic.total_cnt = 0;
  global_statistic.running_thread = 0;
  for (int i = 0; i < 128; i++) {
    global_statistic.thread_statistic[i].thread_id = i;
    global_statistic.thread_statistic[i].mkdir_cnt = 1;
    global_statistic.thread_statistic[i].rmdir_cnt = 1;
    global_statistic.thread_statistic[i].creat_cnt = 1;
    global_statistic.thread_statistic[i].unlink_cnt = 1;
    global_statistic.thread_statistic[i].stat_cnt = 1;
    global_statistic.thread_statistic[i].mkdir_fail_cnt = 1;
    global_statistic.thread_statistic[i].rmdir_fail_cnt = 1;
    global_statistic.thread_statistic[i].creat_fail_cnt = 1;
    global_statistic.thread_statistic[i].unlink_fail_cnt = 1;
    global_statistic.thread_statistic[i].stat_fail_cnt = 1;
    global_statistic.thread_statistic[i].mkdir_time = 1;
    global_statistic.thread_statistic[i].rmdir_time = 1;
    global_statistic.thread_statistic[i].creat_time = 1;
    global_statistic.thread_statistic[i].unlink_time = 1;
    global_statistic.thread_statistic[i].stat_time = 1;
  }
}

void print_statistic() {
  std::unique_lock<std::mutex> lock(printf_mtx);
  std::cout << "total cnt: " << global_statistic.total_cnt << std::endl;

  uint64_t total_mkdir_cnt = 0;
  uint64_t total_mkdir_time = 0;
  uint64_t total_rmdir_cnt = 0;
  uint64_t total_rmdir_time = 0;
  uint64_t total_creat_cnt = 0;
  uint64_t total_creat_time = 0;
  uint64_t total_unlink_cnt = 0;
  uint64_t total_unlink_time = 0;
  uint64_t total_stat_cnt = 0;
  uint64_t total_stat_time = 0;

  uint64_t total_mkdir_fail_cnt = 0;
  uint64_t total_rmdir_fail_cnt = 0;
  uint64_t total_creat_fail_cnt = 0;
  uint64_t total_unlink_fail_cnt = 0;
  uint64_t total_stat_fail_cnt = 0;

  uint64_t total_mkdir_latency = 0;
  double total_mkdir_throughput = 0;
  uint64_t total_rmdir_latency = 0;
  double total_rmdir_throughput = 0;
  uint64_t total_creat_latency = 0;
  double total_creat_throughput = 0;
  uint64_t total_unlink_latency = 0;
  double total_unlink_throughput = 0;
  uint64_t total_stat_latency = 0;
  double total_stat_throughput = 0;

  for (int i = 0; i < FLAGS_thread; i++) {
    total_mkdir_cnt += global_statistic.thread_statistic[i].mkdir_cnt;
    total_mkdir_time += global_statistic.thread_statistic[i].mkdir_time;
    total_rmdir_cnt += global_statistic.thread_statistic[i].rmdir_cnt;
    total_rmdir_time += global_statistic.thread_statistic[i].rmdir_time;
    total_creat_cnt += global_statistic.thread_statistic[i].creat_cnt;
    total_creat_time += global_statistic.thread_statistic[i].creat_time;
    total_unlink_cnt += global_statistic.thread_statistic[i].unlink_cnt;
    total_unlink_time += global_statistic.thread_statistic[i].unlink_time;
    total_stat_cnt += global_statistic.thread_statistic[i].stat_cnt;
    total_stat_time += global_statistic.thread_statistic[i].stat_time;

    total_mkdir_fail_cnt += global_statistic.thread_statistic[i].mkdir_fail_cnt;
    total_rmdir_fail_cnt += global_statistic.thread_statistic[i].rmdir_fail_cnt;
    total_creat_fail_cnt += global_statistic.thread_statistic[i].creat_fail_cnt;
    total_unlink_fail_cnt += global_statistic.thread_statistic[i].unlink_fail_cnt;
    total_stat_fail_cnt += global_statistic.thread_statistic[i].stat_fail_cnt;

    total_mkdir_throughput += (global_statistic.thread_statistic[i].mkdir_cnt / (global_statistic.thread_statistic[i].mkdir_time / 1000000000.0));
    total_rmdir_throughput += (global_statistic.thread_statistic[i].rmdir_cnt / (global_statistic.thread_statistic[i].rmdir_time / 1000000000.0));
    total_creat_throughput += (global_statistic.thread_statistic[i].creat_cnt / (global_statistic.thread_statistic[i].creat_time / 1000000000.0));
    total_unlink_throughput += (global_statistic.thread_statistic[i].unlink_cnt / (global_statistic.thread_statistic[i].unlink_time / 1000000000.0));
    total_stat_throughput += (global_statistic.thread_statistic[i].stat_cnt / (global_statistic.thread_statistic[i].stat_time / 1000000000.0));
  }

  total_mkdir_latency = total_mkdir_time / total_mkdir_cnt;
  total_rmdir_latency = total_rmdir_time / total_rmdir_cnt;
  total_creat_latency = total_creat_time / total_creat_cnt;
  total_unlink_latency = total_unlink_time / total_unlink_cnt;
  total_stat_latency = total_stat_time / total_stat_cnt;

  printf("total mkdir (%ld/%ld) == latency: %ld ns, throughput: %f per sec\n", total_mkdir_cnt - total_mkdir_fail_cnt, total_mkdir_cnt, total_mkdir_latency, total_mkdir_throughput);
  printf("total rmdir (%ld/%ld) == latency: %ld ns, throughput: %f per sec\n", total_rmdir_cnt - total_rmdir_fail_cnt, total_rmdir_cnt, total_rmdir_latency, total_rmdir_throughput);
  printf("total creat (%ld/%ld) == latency: %ld ns, throughput: %f per sec\n", total_creat_cnt - total_creat_fail_cnt, total_creat_cnt, total_creat_latency, total_creat_throughput);
  printf("total unlink (%ld/%ld) == latency: %ld ns, throughput: %f per sec\n", total_unlink_cnt - total_unlink_fail_cnt, total_unlink_cnt, total_unlink_latency, total_unlink_throughput);
  printf("total stat (%ld/%ld) == latency: %ld ns, throughput: %f per sec\n", total_stat_cnt - total_stat_fail_cnt, total_stat_cnt, total_stat_latency, total_stat_throughput);
}

std::vector<std::string> split_path(const std::string& path) {
  if (path.empty()) {
    return std::vector<std::string>();
  }
  std::vector<std::string> components;
  std::istringstream ss_path(path);
  std::string comp;
  while (std::getline(ss_path, comp, '/')) {
    if (!comp.empty()) {
      components.push_back(comp);
    }
  }

  std::vector<std::string> paths;
  std::string cur_path;
  for (const auto& comp : components) {
    cur_path += "/" + comp;
    paths.push_back(cur_path);
  }
  return paths;
}

int test_mkdir(fuseefs::Engine& engine, const char* path,
               uint64_t thread_id) {
  std::this_thread::sleep_for(std::chrono::microseconds(100));
  auto start = std::chrono::high_resolution_clock::now();
  int ret = engine.Mkdir(path, 0, 0, 0755);
  auto end = std::chrono::high_resolution_clock::now();
  global_statistic.thread_statistic[thread_id].mkdir_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  global_statistic.thread_statistic[thread_id].mkdir_cnt++;
  if (ret != 0) {
    global_statistic.thread_statistic[thread_id].mkdir_fail_cnt++;
  }

  if (global_statistic.total_cnt++ % STATISTIC_PRINT_INTERVAL == 0) {
    print_statistic();
  }
  return ret;
}

int test_rmdir(fuseefs::Engine& engine, const char* path,
               uint64_t thread_id) {
  std::this_thread::sleep_for(std::chrono::microseconds(100));
  auto start = std::chrono::high_resolution_clock::now();
  int ret = engine.Rmdir(path, 0, 0);
  auto end = std::chrono::high_resolution_clock::now();
  global_statistic.thread_statistic[thread_id].rmdir_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  global_statistic.thread_statistic[thread_id].rmdir_cnt++;
  if (ret != 0) {
    global_statistic.thread_statistic[thread_id].rmdir_fail_cnt++;
  } 

  if (global_statistic.total_cnt++ % STATISTIC_PRINT_INTERVAL == 0) {
    print_statistic();
  }

  return ret;
}

int test_creat(fuseefs::Engine& engine, const char* path, 
               uint64_t thread_id) {
  std::this_thread::sleep_for(std::chrono::microseconds(100));
  auto start = std::chrono::high_resolution_clock::now();
  int ret = engine.Creat(path, 0, 0, 0755);
  auto end = std::chrono::high_resolution_clock::now();
  global_statistic.thread_statistic[thread_id].creat_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  global_statistic.thread_statistic[thread_id].creat_cnt++;
  if (ret != 0) {
    global_statistic.thread_statistic[thread_id].creat_fail_cnt++;
  } 

  if (global_statistic.total_cnt++ % STATISTIC_PRINT_INTERVAL == 0) {
    print_statistic();
  }

  return ret;
}

int test_unlink(fuseefs::Engine& engine, const char* path, 
                uint64_t thread_id) {
  std::this_thread::sleep_for(std::chrono::microseconds(100));
  auto start = std::chrono::high_resolution_clock::now();
  int ret = engine.Unlink(path, 0, 0);
  auto end = std::chrono::high_resolution_clock::now();
  global_statistic.thread_statistic[thread_id].unlink_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  global_statistic.thread_statistic[thread_id].unlink_cnt++;
  if (ret != 0) {
    global_statistic.thread_statistic[thread_id].unlink_fail_cnt++;
    // printf("unlink failed: %s\n", strerror(-ret));
  } 

  if (global_statistic.total_cnt++ % STATISTIC_PRINT_INTERVAL == 0) {
    print_statistic();
  }

  return ret;
}

int test_stat(fuseefs::Engine& engine, const char* path, 
              uint64_t thread_id) {
  // std::this_thread::sleep_for(std::chrono::microseconds(20));
  fuseefs::Inode inode;
  auto start = std::chrono::high_resolution_clock::now();
  int ret = engine.Stat(path, 0, 0, inode);
  auto end = std::chrono::high_resolution_clock::now();
  global_statistic.thread_statistic[thread_id].stat_time += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  global_statistic.thread_statistic[thread_id].stat_cnt++;
  if (ret != 0) {
    global_statistic.thread_statistic[thread_id].stat_fail_cnt++;
  } 

  if (global_statistic.total_cnt++ % STATISTIC_PRINT_INTERVAL == 0) {
    print_statistic();
  }

  return ret;
}

int test_mkdir_recur(fuseefs::Engine& engine, std::string path,
                     uint64_t thread_id) {
  std::vector<std::string> paths = split_path(path);
  for (const auto& p : paths) {
    int ret = test_mkdir(engine, p.c_str(), thread_id);
    if (ret != 0 && ret != -EEXIST) {
      return ret;
    }
  }
  return 0;
}

// ========= benchmark ========= 

void benchmark(fuseefs::Engine& engine, uint64_t thread_id) {
  int ret = 0;
  struct stat buf;
  const int depth = 4;
  const int total_meta = 251000;
  const int total_file = total_meta / depth;
  const int stat_count = 10000000;

  // std::string basic_path = "/unique_dir." + std::to_string(thread_id);
  std::string basic_path = "";
  
  for (int i = 0; i < total_file; i++) {
    std::string path = basic_path;
    for (int d = 0; d < depth - 1; d++) {
      path += "/dir." + std::to_string(thread_id * total_file + i);
    }
    ret = test_mkdir_recur(engine, path.c_str(), thread_id);
    if (ret != 0) {
      exit(1);
      return ;
    }
    path += "/file." + std::to_string(thread_id * total_file + i);
    ret = test_creat(engine, path.c_str(), thread_id);
    if (ret != 0) {
      exit(1);
      return ;
    }
  }

  // stat
  for (int i = 0; i < total_file; i++) {
    std::string path = basic_path;
    for (int d = 0; d < depth - 1; d++) {
      path += "/dir." + std::to_string(thread_id * total_file + i);
    }
    path += "/file." + std::to_string(thread_id * total_file + i);

    ret = test_stat(engine, path.c_str(), thread_id);
    if (ret != 0) {
      exit(1);
      return ;
    }
  }

  // unlink
  for (int i = 0; i < total_file; i++) {
    std::string path = basic_path;
    for (int d = 0; d < depth - 1; d++) {
      path += "/dir." + std::to_string(thread_id * total_file + i);
    }
    path += "/file." + std::to_string(thread_id * total_file + i);

    ret = test_unlink(engine, path.c_str(), thread_id);
    if (ret != 0) {
      exit(1);
      return ;
    }
  }

  // rmdir
  for (int i = 0; i < total_file; i++) {
    std::string path = basic_path;
    for (int d = 0; d < depth - 1; d++) {
      path += "/dir." + std::to_string(thread_id * total_file + i);
    }
    for (int d = depth - 1; d > 1; d--) {
      path.resize(path.size() - 5 - std::to_string(thread_id * total_file + i).size());
      ret = test_rmdir(engine, path.c_str(), thread_id);
      if (ret != 0) {
        exit(1);
        return ;
      }
    }
  }
}


void bench_throughput_workload_A(fuseefs::Engine& engine, uint64_t thread_id) {
  int ret = 0;
  global_statistic.running_thread++;

  // mkdir private directory
  std::string private_dir = "/dir." + std::to_string(NODE_ID) + "." + std::to_string(thread_id);
  ret = test_mkdir(engine, private_dir.c_str(), thread_id);
  if (ret != 0) {
    printf("mkdir private dir failed: %s\n", strerror(-ret));
    exit(1);
    return ;
  }

  // creat and unlink files
  uint64_t file_count = 100000;
  for (uint64_t i = 0; i < file_count; i++) {
    std::string file_path = private_dir + "/file." + std::to_string(i);
    ret = test_creat(engine, file_path.c_str(), thread_id);
  }
  for (uint64_t i = 0; i < file_count; i++) {
    std::string file_path = private_dir + "/file." + std::to_string(i);
    ret = test_unlink(engine, file_path.c_str(), thread_id);
  }

  // print statistic
  if (global_statistic.running_thread.fetch_sub(1) == 1) {
    print_statistic();
  }
}

void bench_throughput_workload_B(fuseefs::Engine& engine, uint64_t thread_id) {
  int ret = 0;
  struct stat buf;
  const int depth = 8;
  const int total_meta = 200000;
  const int total_file = total_meta / depth;

  global_statistic.running_thread++;

  // std::string basic_path = "/unique_dir." + std::to_string(thread_id);
  std::string basic_path = "";
  
  for (int i = 0; i < total_file; i++) {
    std::string path = basic_path;
    for (int d = 0; d < depth; d++) {
      path += "/dir." + std::to_string(NODE_ID) + "." + std::to_string(thread_id * total_file + i);
    }
    ret = test_mkdir_recur(engine, path.c_str(), thread_id);
  }

  // rmdir
  for (int i = 0; i < total_file; i++) {
    std::string path = basic_path;
    std::vector<std::string> paths;
    for (int d = 0; d < depth; d++) {
      path += "/dir." + std::to_string(thread_id * total_file + i);
      paths.push_back(path);
    }
    paths.reserve(depth);
    for (const std::string& p : paths) {
      ret = test_rmdir(engine, p.c_str(), thread_id);
    }
  }
  // print statistic
  if (global_statistic.running_thread.fetch_sub(1) == 1) {
    print_statistic();
  }
}


void bench_latency(fuseefs::Engine& engine, uint64_t thread_id) {
  int ret = 0;
  struct stat buf;
  const int depth = 2;
  const int total_meta = 10000;
  const int total_file = total_meta / depth;

  // std::string basic_path = "/unique_dir." + std::to_string(thread_id);
  std::string basic_path = "";
  
  for (int i = 0; i < total_file; i++) {
    std::string path = basic_path;
    for (int d = 0; d < depth - 1; d++) {
      path += "/dir." + std::to_string(thread_id * total_file + i);
    }
    ret = test_mkdir_recur(engine, path.c_str(), thread_id);
    path += "/file." + std::to_string(thread_id * total_file + i);
    ret = test_creat(engine, path.c_str(), thread_id);
  }

  // unlink
  for (int i = 0; i < total_file; i++) {
    std::string path = basic_path;
    for (int d = 0; d < depth - 1; d++) {
      path += "/dir." + std::to_string(thread_id * total_file + i);
    }
    path += "/file." + std::to_string(thread_id * total_file + i);

    ret = test_unlink(engine, path.c_str(), thread_id);
  }

  // rmdir
  for (int i = 0; i < total_file; i++) {
    std::string path = basic_path;
    for (int d = 0; d < depth - 1; d++) {
      path += "/dir." + std::to_string(thread_id * total_file + i);
    }
    for (int d = depth - 1; d >= 1; d--) {
      ret = test_rmdir(engine, path.c_str(), thread_id);
      path.resize(path.size() - 5 - std::to_string(thread_id * total_file + i).size());
    }
  }
  print_statistic();
}

void bench_stat_load(fuseefs::Engine& engine, uint64_t thread_id) {
  int ret = 0;
  global_statistic.running_thread++;

  const uint64_t depth = 8;
  const uint64_t total_meta = 500000;
  const uint64_t total_file = total_meta / depth;

  for (uint64_t i = 0; i < total_file; i++) {
    std::string path = "";
    for (int d = 0; d < depth - 1; d++) {
      path += "/dir." + std::to_string(i);
    }
    ret = test_mkdir_recur(engine, path.c_str(), thread_id);
    path += "/file." + std::to_string(i);
    ret = test_creat(engine, path.c_str(), thread_id);
  }
}

void bench_stat_load_with_ap(fuseefs::Engine& engine, uint64_t thread_id) {
  int ret = 0;
  global_statistic.running_thread++;

  const uint64_t depth = 8;
  const uint64_t total_meta = 500000;
  const uint64_t total_file = total_meta / depth;
  const uint64_t group_size = 100;
  const uint64_t total_group = total_file / group_size;

  for (uint64_t i = 0; i < total_file; i++) {
    std::string path = "";
    for (int d = 0; d < depth - 1; d++) {
      path += "/dir." + std::to_string(i);
    }
    ret = test_mkdir_recur(engine, path.c_str(), thread_id);
    path += "/file." + std::to_string(i);
    ret = test_creat(engine, path.c_str(), thread_id);
    if (i > 0 && i % group_size == 0) {
      uint64_t cur_group = (i - 1) / group_size;
      for (uint64_t epoch = 0; epoch < 100; epoch++) {
        for (uint64_t j = 0; j < group_size; j++) {
          std::string path = "";
          for (int d = 0; d < depth - 1; d++) {
            path += "/dir." + std::to_string(cur_group * group_size + j);
          }
          path += "/file." + std::to_string(cur_group * group_size + j);
          // printf("stat path: %s\n", path.c_str());
          ret = test_stat(engine, path.c_str(), thread_id);
        }
      }
      // std::this_thread::sleep_for(std::chrono::seconds(3));
    }
  }
}

void bench_stat_load_with_bad_ap(fuseefs::Engine& engine, uint64_t thread_id) {
  int ret = 0;
  global_statistic.running_thread++;

  const uint64_t depth = 8;
  const uint64_t total_meta = 500000;
  const uint64_t total_file = total_meta / depth;
  const uint64_t group_size = 500;
  const uint64_t total_group = total_file / group_size;

  for (uint64_t i = 0; i < total_file; i++) {
    std::string path = "";
    for (int d = 0; d < depth - 1; d++) {
      path += "/dir." + std::to_string(i);
    }
    ret = test_mkdir_recur(engine, path.c_str(), thread_id);
    path += "/file." + std::to_string(i);
    ret = test_creat(engine, path.c_str(), thread_id);
    if (i > 0 && i % group_size == 0) {
      uint64_t cur_group = (i - 1) / group_size;
      for (uint64_t epoch = 0; epoch < 1000; epoch++) {
        for (uint64_t j = 0; j < group_size; j++) {
          std::string path = "";
          for (int d = 0; d < depth - 1; d++) {
            path += "/dir." + std::to_string(cur_group * group_size + j);
          }
          path += "/file." + std::to_string(cur_group * group_size + j);
          ret = test_stat(engine, path.c_str(), thread_id);
        }
      }
      std::this_thread::sleep_for(std::chrono::seconds(5));
    }
  }
}

void bench_stat(fuseefs::Engine& engine, uint64_t thread_id) {
  srand(time(NULL));
  int ret = 0;
  global_statistic.running_thread++;

  const uint64_t depth = 8;
  const uint64_t total_meta = 500000;
  const uint64_t total_file = total_meta / depth;
  const uint64_t stat_time = 100000;
  const uint64_t group_size = 1000;
  const uint64_t total_group = total_file / group_size;

  uint64_t cur_group = 0;

  global_statistic.thread_statistic[thread_id].ext = new std::vector<uint64_t>(stat_time, 0);
  std::vector<uint64_t>& thread_stat_lats = *reinterpret_cast<std::vector<uint64_t>*>(global_statistic.thread_statistic[thread_id].ext);
  uint64_t cur_total_lat = 0;
  for (uint64_t i = 0; i < stat_time; i++) {
    if (random() % 1000 < 10) {
      // change group
      cur_group = random() % total_group;
    } 
    // random select a file
    uint64_t file_id = cur_group * group_size + random() % group_size;
    std::string path = "";
    for (int d = 0; d < depth - 1; d++) {
      path += "/dir." + std::to_string(file_id);
    }
    path += "/file." + std::to_string(file_id);
    test_stat(engine, path.c_str(), thread_id);
    thread_stat_lats[i] = global_statistic.thread_statistic[thread_id].stat_time - cur_total_lat;
    cur_total_lat = global_statistic.thread_statistic[thread_id].stat_time;
  }

  if (global_statistic.running_thread.fetch_sub(1) == 1) {
    // engine.ClearMDBCache();
    print_statistic();
    // latency
    std::vector<uint64_t> all_lats;
    for (uint64_t thread = 0; thread < FLAGS_thread; thread++) {
      std::vector<uint64_t>* thread_stat_lats = reinterpret_cast<std::vector<uint64_t>*>(global_statistic.thread_statistic[thread].ext);
      all_lats.insert(all_lats.end(), thread_stat_lats->begin(), thread_stat_lats->end());
      delete thread_stat_lats;
    }
    std::sort(all_lats.begin(), all_lats.end());
    uint64_t p50 = all_lats[all_lats.size() / 2];
    uint64_t p90 = all_lats[all_lats.size() * 9 / 10];
    uint64_t p99 = all_lats[all_lats.size() * 99 / 100];
    uint64_t p999 = all_lats[all_lats.size() * 999 / 1000];
    uint64_t p9999 = all_lats[all_lats.size() * 9999 / 10000];
    printf("latency: p50: %ld, p90: %ld, p99: %ld, p999: %ld, p9999: %ld\n", p50, p90, p99, p999, p9999);
    // cache hit rate
    printf("cache hit: %ld/%ld (%f)\n", search_hit_count.load(), search_count.load(), (double)search_hit_count.load()/ search_count.load());
    // network round trip
    printf("network round trip: %ld\n", rtt_.load());
  }
}

void BenchThread(fuseefs::Engine& engine, uint64_t thread_id) {
  // zenith::BindToCore(thread_id % 40);  
  engine.InitThread(thread_id);
  bench_throughput_workload_B(engine, thread_id);
}

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  search_hit_count = 0;
  search_count = 0;
  rtt_ = 0;

  // initial engine
  GlobalConfig fusee_client_conf;
  int ret = load_config(FLAGS_fusee_client_conf.c_str(), &fusee_client_conf);
  assert(ret == 0);

  fuseefs::Engine engine(fusee_client_conf, FLAGS_central_erpc_hostname, FLAGS_central_erpc_port,
                         FLAGS_local_erpc_hostname, FLAGS_local_erpc_port);

  init_statistic();
  std::vector<std::thread> threads;
  for (uint64_t i = 0; i < FLAGS_thread; i++) {
    std::cout << "create thread " << i << std::endl;
    threads.push_back(std::thread(BenchThread, std::ref(engine), i));
  }
  for (uint64_t i = 0; i < FLAGS_thread; i++) {
    threads[i].join();
  }
}