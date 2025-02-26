#ifdef ASSERT
#include <cassert>
#endif

#include <pthread.h>
#include "fs/common/thread.h"

namespace fuseefs
{

std::vector<uint64_t> GetCpuAffinity() {
  std::vector<uint64_t> cores;
  cpu_set_t cpuset;
  
  int ret = pthread_getaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
#ifdef ASSERT
  assert(ret == 0);
#endif
  for (uint64_t i = 0; i < CPU_SETSIZE; ++i) {
    if (CPU_ISSET(i, &cpuset)) {
      cores.push_back(i);
    }
  }
  return cores;
}

void EnableOnCores(const std::vector<uint64_t>& core_ids) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  for (uint64_t core_id : core_ids) {
    CPU_SET(core_id, &cpuset);
  }
  int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
#ifdef ASSERT
  assert(ret == 0);
#endif
}

void BindToCore(uint64_t core_id) {
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(core_id, &cpuset);
  int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
#ifdef ASSERT
  assert(ret == 0);
#endif
}

} // namespace fuseefs