#ifndef FUSEEFS_COMMON_TYPE_INCLUDE_
#define FUSEEFS_COMMON_TYPE_INCLUDE_

#include <cstdint>

#define kInodeIdGroupSize (1024 * 1024)
#define kMaxLockInodeNum 4
#define kRootInodeId 0
#define kInvalidInodeId UINT64_MAX 

namespace fuseefs
{

struct Inode {
  // basic inode info
  uint64_t id;
  uint64_t size;
  uint64_t atime;
  uint64_t mtime;
  uint64_t ctime;
  uint64_t children;
  uint32_t mode;
  uint32_t uid;
  uint32_t gid;
} __attribute__((packed));

} // namespace fuseefs


#endif // FUSEEFS_COMMON_TYPE_INCLUDE_