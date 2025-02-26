#ifndef FUSEEFS_COMMON_NONCOPYABLE_INCLUDE_
#define FUSEEFS_COMMON_NONCOPYABLE_INCLUDE_

namespace fuseefs 
{

/**
 * Disable copy constructor and copy assignment to avoid accidental copy
 */
class noncopyable {
 public:
  noncopyable() = default;
  noncopyable(const noncopyable &) = delete;
  noncopyable &operator=(const noncopyable &) = delete;
};

}

#endif  // FUSEEFS_COMMON_NONCOPYABLE_INCLUDE_