#ifndef FUSEEFS_COMMON_THREAD_INCLUDE_
#define FUSEEFS_COMMON_THREAD_INCLUDE_

#include <vector>
#include <cstdint>

namespace fuseefs
{

std::vector<uint64_t> GetCpuAffinity();

void EnableOnCores(const std::vector<uint64_t>& core_ids);

void BindToCore(uint64_t core_id);

} // namespace fuseefs


#endif // FUSEEFS_COMMON_THREAD_INCLUDE_