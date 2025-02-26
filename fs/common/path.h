#ifndef FUSEEFS_FS_PATH_INCLUDE_
#define FUSEEFS_FS_PATH_INCLUDE_

#ifndef _OUT
#define _OUT
#endif

#include <cstdint>
#include <string>
#include <vector>

namespace fuseefs
{

std::vector<std::string> SplitPath(const std::string& path);

std::pair<std::string, std::string> SeparateLastComponent(const std::string& path);

std::string FormatPath(const std::string& path);

} // namespace fuseefs

#endif // FUSEEFS_FS_PATH_INCLUDE_
