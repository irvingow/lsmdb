//
// Created by 刘文景 on 2021/3/29.
//

#include "lsmdb/status.h"

#include <cstdio>

#include "port/port.h"

namespace lsmdb {

const char *Status::CopyState(const char *state) {
  // OK status has a null state_. Otherwise, state_ is a new[] array
  // of the following form:
  //    state_[0..3] == length of message
  //    state_[4]    == code
  //    state_[5..]  == message
  uint32_t size;
  std::memcpy(&size, state, sizeof(size));
  char *result = new char[size + 5];
  std::memcpy(result, state, size + 5);
  return result;
}

Status::Status(lsmdb::Status::Code code, const lsmdb::Slice &msg,
               const lsmdb::Slice &msg2) {
  assert(code != kOk);
  auto len1 = static_cast<uint32_t>(msg.size());
  auto len2 = static_cast<uint32_t>(msg2.size());
  const uint32_t size = len1 + (len2 ? (2 + len2) : 0);
  char *result = new char[size + 5];
  std::memcpy(result, &size, sizeof(size));
  result[4] = static_cast<char>(code);
  std::memcpy(result + 5, msg.data(), len1);
  if (len2) {
    result[5 + len1] = ':';
    result[6 + len1] = ' ';
    std::memcpy(result + 7 + len1, msg2.data(), len2);
  }
  state_ = result;
}

std::string Status::ToString() const {
  if (state_ == nullptr) {
    return "OK";
  } else {
    char tmp[30];
    const char *type;
    switch (code()) {
      case kOk:
        type = "OK";
        break;
      case kNotFound:
        type = "NotFound: ";
        break;
      case kCorruption:
        type = "Corruption: ";
        break;
      case kNotSupported:
        type = "Not implemented: ";
        break;
      case kInvalidArgument:
        type = "Invalid argument: ";
        break;
      case kIOError:
        type = "IO error: ";
        break;
      default:
        std::snprintf(tmp, sizeof(tmp),
                      "Unknown code(%d): ", static_cast<int>(code()));
        type = tmp;
        break;
    }
    std::string result(type);
    uint32_t length;
    std::memcpy(&length, state_, sizeof(length));
    result.append(state_ + 5, length);
    return result;
  }
}

}  // namespace lsmdb