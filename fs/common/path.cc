#include <sstream>

#include "fs/common/path.h"

namespace fuseefs
{

std::vector<std::string> SplitPath(const std::string& path) {
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
  return components;
}

std::pair<std::string, std::string> SeparateLastComponent(const std::string& path) {
  if (path.empty()) {
    return {"", ""};
  }
  size_t pos = path.find_last_of('/');
  if (pos == std::string::npos) {
    return {"", path};
  }
  // Separate the path into the directory part and the last component
  std::string directory = path.substr(0, pos);
  std::string last_component = path.substr(pos + 1);

  return {directory, last_component};
}

std::string FormatPath(const std::string& path) {
  std::string formatted_path = path;
  if (formatted_path.empty()) {
    return formatted_path;
  }
  if (formatted_path.front() != '/') {
    formatted_path.insert(formatted_path.begin(), '/');
  }
  if (formatted_path.back() == '/') {
    formatted_path.pop_back();
  }
  for (size_t i = 1; i < formatted_path.size(); ++i) {
    if (formatted_path[i] == '/' && formatted_path[i - 1] == '/') {
      formatted_path.erase(i--, 1);
    }
  }
  return formatted_path;
}

} // namespace fuseefs
