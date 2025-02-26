#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>

#define FUSEEFS_MOUNT_POINT "/fuseefs"

int main() {
  // Test mkdir
  std::string dirPath = FUSEEFS_MOUNT_POINT;
  dirPath += "/testDir";
  int ret;

  if ((ret = mkdir(dirPath.c_str(), 0777)) < 0) {
    std::cerr << "mkdir failed: "<< ret << "\n";
    return 1;
  } else {
    std::cout << "mkdir success: "<< ret << "\n"; 
  }

  // Test creat
  std::string filePath = dirPath;
  filePath += "/testFile";
  if ((ret = creat(filePath.c_str(), 0666)) < 0) {
    std::cerr << "creat failed: "<< ret << "\n";
    return 1;
  } else {
    std::cout << "creat success: "<< ret << "\n";
  }

  // Test stat
  struct stat st;
  if ((ret = stat(filePath.c_str(), &st)) < 0) {
    std::cerr << "stat failed: "<< ret << "\n";
    return 1;
  } else {
    std::cout << "stat success: "<< ret << "\n";
    std::cout << "uid: " << st.st_uid << std::endl;
    std::cout << "gid: " << st.st_gid << std::endl;
    std::cout << "mode: " << st.st_mode << std::endl;
    std::cout << "atime: " << st.st_atime << std::endl;
    std::cout << "mtime: " << st.st_mtime << std::endl;
    std::cout << "ctime: " << st.st_ctime << std::endl;
  }

  if ((ret = stat(dirPath.c_str(), &st)) < 0) {
    std::cerr << "stat failed: "<< ret << "\n";
    return 1;
  } else {
    std::cout << "stat success: "<< ret << "\n";
    std::cout << "uid: " << st.st_uid << std::endl;
    std::cout << "gid: " << st.st_gid << std::endl;
    std::cout << "mode: " << st.st_mode << std::endl;
    std::cout << "atime: " << st.st_atime << std::endl;
    std::cout << "mtime: " << st.st_mtime << std::endl;
    std::cout << "ctime: " << st.st_ctime << std::endl;
  }

  // Test remove
  if ((ret = remove(filePath.c_str())) < 0) {
    std::cerr << "remove failed: "<< ret << "\n";
    return 1;
  } else {
    std::cout << "remove success: "<< ret << "\n";
  }

  // Test rmdir
  if ((ret = rmdir(dirPath.c_str())) < 0) {
    std::cerr << "rmdir failed: "<< ret << "\n";
    return 1;
  } else {
    std::cout << "rmdir success: "<< ret << "\n";
  }

  if ((ret = stat(filePath.c_str(), &st)) < 0) {
    std::cerr << "stat failed: "<< ret << "\n";
  } else {
    std::cout << "stat success: "<< ret << "\n";
    std::cout << "uid: " << st.st_uid << std::endl;
    std::cout << "gid: " << st.st_gid << std::endl;
    std::cout << "mode: " << st.st_mode << std::endl;
    std::cout << "atime: " << st.st_atime << std::endl;
    std::cout << "mtime: " << st.st_mtime << std::endl;
    std::cout << "ctime: " << st.st_ctime << std::endl;
    return 1;
  }

  if ((ret = stat(dirPath.c_str(), &st)) < 0) {
    std::cerr << "stat failed: "<< ret << "\n";
  } else {
    std::cout << "stat success: "<< ret << "\n";
    std::cout << "uid: " << st.st_uid << std::endl;
    std::cout << "gid: " << st.st_gid << std::endl;
    std::cout << "mode: " << st.st_mode << std::endl;
    std::cout << "atime: " << st.st_atime << std::endl;
    std::cout << "mtime: " << st.st_mtime << std::endl;
    std::cout << "ctime: " << st.st_ctime << std::endl;
    return 1;
  }


  std::cout << "All tests passed\n";
  return 0;
}