//
// Created by 刘文景 on 2021/3/31.
//

#include "lsmdb/env.h"

#include <cstdarg>
#include <vector>
#include <memory>

// This workaround can be removed when leveldb::Env::DeleteFile is removed.
// See env.h for justification.
#if defined(_WIN32) && defined(LSMDB_DELETEFILE_UNDEFINED)
#undef DeleteFile
#endif

namespace lsmdb {

Env::Env() = default;

Env::~Env() = default;

Status Env::NewAppendableFile(const std::string &filename, WritableFile **result) {
  return Status::NotSupported("NewAppendableFile", filename);
}

SequentialFile::~SequentialFile() = default;

RandomAccessFile::~RandomAccessFile() = default;

WritableFile::~WritableFile() = default;

Logger::~Logger() = default;

FileLock::~FileLock() = default;

void Log(Logger* info_log, const char* format, ...) {
  if (info_log != nullptr) {
    std::va_list ap;
    va_start(ap, format);
    info_log->Logv(format, ap);
    va_end(ap);
  }
}

static Status DoWriteStringToFile(Env* env, const Slice& data, const std::string& filename, bool should_sync) {
  WritableFile* file;
  auto s = env->NewWritableFile(filename, &file);
  if (!s.ok()) {
    return s;
  }
  s = file->Append(data);
  if (s.ok() && should_sync) {
    s = file->Sync();
  }
  if (s.ok()) {
    s = file->Close();
  }
  delete file; // Will auto-close if we did not close above
  if (!s.ok()) {
    env->RemoveFile(filename);
  }
  return s;
}

Status WriteStringToFile(Env* env, const Slice& data,
                         const std::string& fname) {
  return DoWriteStringToFile(env, data, fname, false);
}

Status WriteStringToFileSync(Env* env, const Slice& data,
                             const std::string& fname) {
  return DoWriteStringToFile(env, data, fname, true);
}


Status ReadFileToString(Env* env, const std::string& filename, std::string* data) {
  data->clear();
  SequentialFile* file;
  auto s = env->NewSequentialFile(filename, &file);
  // file MUST not be used anymore.
  std::unique_ptr<SequentialFile> sp_file(file);
  if (!s.ok())
    return s;
  static const int kBufferSize = 8192;
  std::vector<char> space(8192);
  while (true) {
    Slice fragment;
    s = sp_file->Read(kBufferSize, &fragment, space.data());
    if (!s.ok())
      break;
    data->append(fragment.data(), fragment.size());
    if (fragment.empty())
      break;
  }
  return s;
}

EnvWapper::~EnvWapper() = default;

} // namespace lsmdb