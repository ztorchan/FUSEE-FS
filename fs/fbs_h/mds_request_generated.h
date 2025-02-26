// automatically generated by the FlatBuffers compiler, do not modify


#ifndef FLATBUFFERS_GENERATED_MDSREQUEST_FUSEEFS_H_
#define FLATBUFFERS_GENERATED_MDSREQUEST_FUSEEFS_H_

#include "flatbuffers/flatbuffers.h"

// Ensure the included flatbuffers.h is the same version as when this file was
// generated, otherwise it may not be compatible.
static_assert(FLATBUFFERS_VERSION_MAJOR == 24 &&
              FLATBUFFERS_VERSION_MINOR == 3 &&
              FLATBUFFERS_VERSION_REVISION == 25,
             "Non-compatible flatbuffers version included");

namespace fuseefs {

struct MDRequest;
struct MDRequestBuilder;

enum MDOpType : int8_t {
  MDOpType_None = 0,
  MDOpType_Mkdir = 1,
  MDOpType_Rmdir = 2,
  MDOpType_Creat = 3,
  MDOpType_Unlink = 4,
  MDOpType_Stat = 5,
  MDOpType_Rename = 6,
  MDOpType_MIN = MDOpType_None,
  MDOpType_MAX = MDOpType_Rename
};

inline const MDOpType (&EnumValuesMDOpType())[7] {
  static const MDOpType values[] = {
    MDOpType_None,
    MDOpType_Mkdir,
    MDOpType_Rmdir,
    MDOpType_Creat,
    MDOpType_Unlink,
    MDOpType_Stat,
    MDOpType_Rename
  };
  return values;
}

inline const char * const *EnumNamesMDOpType() {
  static const char * const names[8] = {
    "None",
    "Mkdir",
    "Rmdir",
    "Creat",
    "Unlink",
    "Stat",
    "Rename",
    nullptr
  };
  return names;
}

inline const char *EnumNameMDOpType(MDOpType e) {
  if (::flatbuffers::IsOutRange(e, MDOpType_None, MDOpType_Rename)) return "";
  const size_t index = static_cast<size_t>(e);
  return EnumNamesMDOpType()[index];
}

struct MDRequest FLATBUFFERS_FINAL_CLASS : private ::flatbuffers::Table {
  typedef MDRequestBuilder Builder;
  enum FlatBuffersVTableOffset FLATBUFFERS_VTABLE_UNDERLYING_TYPE {
    VT_OP = 4,
    VT_UID = 6,
    VT_GID = 8,
    VT_PATH = 10,
    VT_NEWPATH = 12,
    VT_MODE = 14
  };
  fuseefs::MDOpType op() const {
    return static_cast<fuseefs::MDOpType>(GetField<int8_t>(VT_OP, 0));
  }
  bool mutate_op(fuseefs::MDOpType _op = static_cast<fuseefs::MDOpType>(0)) {
    return SetField<int8_t>(VT_OP, static_cast<int8_t>(_op), 0);
  }
  uint32_t uid() const {
    return GetField<uint32_t>(VT_UID, 0);
  }
  bool mutate_uid(uint32_t _uid = 0) {
    return SetField<uint32_t>(VT_UID, _uid, 0);
  }
  uint32_t gid() const {
    return GetField<uint32_t>(VT_GID, 0);
  }
  bool mutate_gid(uint32_t _gid = 0) {
    return SetField<uint32_t>(VT_GID, _gid, 0);
  }
  const ::flatbuffers::String *path() const {
    return GetPointer<const ::flatbuffers::String *>(VT_PATH);
  }
  ::flatbuffers::String *mutable_path() {
    return GetPointer<::flatbuffers::String *>(VT_PATH);
  }
  const ::flatbuffers::String *newpath() const {
    return GetPointer<const ::flatbuffers::String *>(VT_NEWPATH);
  }
  ::flatbuffers::String *mutable_newpath() {
    return GetPointer<::flatbuffers::String *>(VT_NEWPATH);
  }
  uint32_t mode() const {
    return GetField<uint32_t>(VT_MODE, 0);
  }
  bool mutate_mode(uint32_t _mode = 0) {
    return SetField<uint32_t>(VT_MODE, _mode, 0);
  }
  bool Verify(::flatbuffers::Verifier &verifier) const {
    return VerifyTableStart(verifier) &&
           VerifyField<int8_t>(verifier, VT_OP, 1) &&
           VerifyField<uint32_t>(verifier, VT_UID, 4) &&
           VerifyField<uint32_t>(verifier, VT_GID, 4) &&
           VerifyOffset(verifier, VT_PATH) &&
           verifier.VerifyString(path()) &&
           VerifyOffset(verifier, VT_NEWPATH) &&
           verifier.VerifyString(newpath()) &&
           VerifyField<uint32_t>(verifier, VT_MODE, 4) &&
           verifier.EndTable();
  }
};

struct MDRequestBuilder {
  typedef MDRequest Table;
  ::flatbuffers::FlatBufferBuilder &fbb_;
  ::flatbuffers::uoffset_t start_;
  void add_op(fuseefs::MDOpType op) {
    fbb_.AddElement<int8_t>(MDRequest::VT_OP, static_cast<int8_t>(op), 0);
  }
  void add_uid(uint32_t uid) {
    fbb_.AddElement<uint32_t>(MDRequest::VT_UID, uid, 0);
  }
  void add_gid(uint32_t gid) {
    fbb_.AddElement<uint32_t>(MDRequest::VT_GID, gid, 0);
  }
  void add_path(::flatbuffers::Offset<::flatbuffers::String> path) {
    fbb_.AddOffset(MDRequest::VT_PATH, path);
  }
  void add_newpath(::flatbuffers::Offset<::flatbuffers::String> newpath) {
    fbb_.AddOffset(MDRequest::VT_NEWPATH, newpath);
  }
  void add_mode(uint32_t mode) {
    fbb_.AddElement<uint32_t>(MDRequest::VT_MODE, mode, 0);
  }
  explicit MDRequestBuilder(::flatbuffers::FlatBufferBuilder &_fbb)
        : fbb_(_fbb) {
    start_ = fbb_.StartTable();
  }
  ::flatbuffers::Offset<MDRequest> Finish() {
    const auto end = fbb_.EndTable(start_);
    auto o = ::flatbuffers::Offset<MDRequest>(end);
    return o;
  }
};

inline ::flatbuffers::Offset<MDRequest> CreateMDRequest(
    ::flatbuffers::FlatBufferBuilder &_fbb,
    fuseefs::MDOpType op = fuseefs::MDOpType_None,
    uint32_t uid = 0,
    uint32_t gid = 0,
    ::flatbuffers::Offset<::flatbuffers::String> path = 0,
    ::flatbuffers::Offset<::flatbuffers::String> newpath = 0,
    uint32_t mode = 0) {
  MDRequestBuilder builder_(_fbb);
  builder_.add_mode(mode);
  builder_.add_newpath(newpath);
  builder_.add_path(path);
  builder_.add_gid(gid);
  builder_.add_uid(uid);
  builder_.add_op(op);
  return builder_.Finish();
}

inline ::flatbuffers::Offset<MDRequest> CreateMDRequestDirect(
    ::flatbuffers::FlatBufferBuilder &_fbb,
    fuseefs::MDOpType op = fuseefs::MDOpType_None,
    uint32_t uid = 0,
    uint32_t gid = 0,
    const char *path = nullptr,
    const char *newpath = nullptr,
    uint32_t mode = 0) {
  auto path__ = path ? _fbb.CreateString(path) : 0;
  auto newpath__ = newpath ? _fbb.CreateString(newpath) : 0;
  return fuseefs::CreateMDRequest(
      _fbb,
      op,
      uid,
      gid,
      path__,
      newpath__,
      mode);
}

inline const fuseefs::MDRequest *GetMDRequest(const void *buf) {
  return ::flatbuffers::GetRoot<fuseefs::MDRequest>(buf);
}

inline const fuseefs::MDRequest *GetSizePrefixedMDRequest(const void *buf) {
  return ::flatbuffers::GetSizePrefixedRoot<fuseefs::MDRequest>(buf);
}

inline MDRequest *GetMutableMDRequest(void *buf) {
  return ::flatbuffers::GetMutableRoot<MDRequest>(buf);
}

inline fuseefs::MDRequest *GetMutableSizePrefixedMDRequest(void *buf) {
  return ::flatbuffers::GetMutableSizePrefixedRoot<fuseefs::MDRequest>(buf);
}

inline bool VerifyMDRequestBuffer(
    ::flatbuffers::Verifier &verifier) {
  return verifier.VerifyBuffer<fuseefs::MDRequest>(nullptr);
}

inline bool VerifySizePrefixedMDRequestBuffer(
    ::flatbuffers::Verifier &verifier) {
  return verifier.VerifySizePrefixedBuffer<fuseefs::MDRequest>(nullptr);
}

inline void FinishMDRequestBuffer(
    ::flatbuffers::FlatBufferBuilder &fbb,
    ::flatbuffers::Offset<fuseefs::MDRequest> root) {
  fbb.Finish(root);
}

inline void FinishSizePrefixedMDRequestBuffer(
    ::flatbuffers::FlatBufferBuilder &fbb,
    ::flatbuffers::Offset<fuseefs::MDRequest> root) {
  fbb.FinishSizePrefixed(root);
}

}  // namespace fuseefs

#endif  // FLATBUFFERS_GENERATED_MDSREQUEST_FUSEEFS_H_
