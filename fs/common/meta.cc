#include "fs/common/meta.h"

#include <sys/stat.h>

namespace fuseefs
{

bool IsFile(uint32_t mode) {
  return S_ISREG(mode) != 0;
}

bool IsDir(uint32_t mode) {
  return S_ISDIR(mode) != 0;
}

bool CheckReadPermission(const Inode& inode, uint32_t uid, uint32_t gid) {
  if (uid == 0) {
    return true;
  }
  if (uid == inode.uid) {
    return (inode.mode & S_IRUSR) != 0;
  }
  if (gid == inode.gid) {
    return (inode.mode & S_IRGRP) != 0;
  }
  return (inode.mode & S_IROTH) != 0;
}

bool CheckWritePermission(const Inode& inode, uint32_t uid, uint32_t gid) {
  if (uid == 0) {
    return true;
  }
  if (uid == inode.uid) {
    return (inode.mode & S_IWUSR) != 0;
  }
  if (gid == inode.gid) {
    return (inode.mode & S_IWGRP) != 0;
  }
  return (inode.mode & S_IWOTH) != 0;
}

} // namespace fuseefs
