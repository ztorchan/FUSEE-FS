#ifndef FUSEEFS_FS_META_INCLUDE_
#define FUSEEFS_FS_META_INCLUDE_

#include <cstdint>
#include <memory>

#include "fs/common/type.h"

namespace fuseefs
{

bool IsFile(uint32_t mode);

bool IsDir(uint32_t mode);

bool CheckReadPermission(const Inode& inode, uint32_t uid, uint32_t gid);

bool CheckWritePermission(const Inode& inode, uint32_t uid, uint32_t gid);

} // namespace fuseefs


#endif // FUSEEFS_FS_META_INCLUDE_