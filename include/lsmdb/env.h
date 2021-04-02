//
// Created by 刘文景 on 2021/3/29.
//

#ifndef STORAGE_LSMDB_INCLUDE_ENV_H_
#define STORAGE_LSMDB_INCLUDE_ENV_H_

#include <cstdarg>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "lsmdb/export.h"
#include "lsmdb/status.h"
#include "util/noncopyable.h"

// This workaround can be removed when leveldb::Env::DeleteFile is removed.
#if defined(_WIN32)
// On Windows, the method name DeleteFile (below) introduces the risk of
// triggering undefined behavior by exposing the compiler to different
// declarations of the Env class in different translation units.
//
// This is because <windows.h>, a fairly popular header file for Windows
// applications, defines a DeleteFile macro. So, files that include the Windows
// header before this header will contain an altered Env declaration.
//
// This workaround ensures that the compiler sees the same Env declaration,
// independently of whether <windows.h> was included.
#if defined(DeleteFile)
#undef DeleteFile
#define LEVELDB_DELETEFILE_UNDEFINED
#endif  // defined(DeleteFile)
#endif  // defined(_WIN32)

namespace lsmdb {

class FileLock;
class Logger;
class RandomAccessFile;
class SequentialFile;
class Slice;
class WritableFile;

class LSMDB_EXPORT Env : public noncopyable {
 public:
  Env();

  virtual ~Env();

  // Return a default environment suitable for the current operating
  // system. Sophisticated users may wish to provide their own Env
  // implementation instead of relying on this default environment.
  //
  /// The result of Default() belongs to leveldb and must never be deleted.
  static Env* Default();

  // Create an object that sequentially reads the file with the specified name.
  // On success, stores a pointer to the new file in *result and returns OK.
  // On failure stores nullptr in *result and returns non-OK. If the file does
  // not exist, returns a non-OK status. Implementations should return a
  // NotFound status when the file does not exist.
  virtual Status NewSequentialFile(const std::string& filename,
                                   SequentialFile** result) = 0;

  // Create an object supporting random-access reads from the file with the
  // specified name. On success, stores a pointer to the new file in *result
  // and returns OK. On failure stores nullptr in *result and returns non-OK.
  // If the file does not exist, returns a non-OK status. Implementation
  // should return a NotFound status when the file does not exist.
  //
  /// The returned file may be concurrently accessed by multiple threads.
  virtual Status NewRandomAccessFile(const std::string& filename,
                                     RandomAccessFile** result) = 0;

  // Create an object that writes to a new file with the specified name.
  // Deletes any existing file with the same name and creates a new file.
  // On success, stores a pointer to the new file in *result and returns
  // OK. On failure stores nullptr in *result and returns non-OK.
  //
  /// The returned file will only be accessed by one thread at a time.
  virtual Status NewWritableFile(const std::string& filename,
                                 WritableFile** result) = 0;

  // Create an object that either appends to an existing file, or
  // writes to a new file (if the file does not exist to begin with).
  // On success, stores a pointer to the new file in *result and
  // returns OK. On failure stores nullptr in *result and returns
  // non-OK.
  //
  // The returned file will not be accessed by one thread at a time.
  //
  // May return an IsNotSupportedError error if this Env does
  // not allow appending to an existing file. Users of Env (including
  // the lsmdb implementation) must be prepared to deal with
  // an Env that does not support appending.
  virtual Status NewAppendableFile(const std::string& filename,
                                   WritableFile** result);

  // Returns true iff the named file exists.
  virtual bool FileExists(const std::string& filename) = 0;

  // Store in *result the names of the children of the
  // specified directory.
  // The names are relative to "dir".
  // Original contents of *result are dropped.
  virtual Status GetChildren(const std::string& dir,
                             std::vector<std::string>* result) = 0;

  // Delete the named file.
  virtual Status RemoveFile(const std::string& filename) = 0;

  // Create the specified directory.
  virtual Status CreateDir(const std::string& dirname) = 0;

  // Delete the specified directory.
  virtual Status RemoveDir(const std::string& dirname) = 0;

  // Store the size of filename in *file_size.
  virtual Status GetFileSize(const std::string& filename,
                             uint64_t* file_size) = 0;

  // Rename file src to target.
  virtual Status RenameFile(const std::string& src,
                            const std::string& target) = 0;

  // Lock the specified file. Used to prevent concurrent access to
  // the same db by multiple processes. On failure, stores nullptr
  // in *lock and returns non-OK.
  //
  // On success, stores a pointer to the object that represents the
  // acquired lock in *lock and returns OK. The caller should call
  // UnlockFile(*lock) to release the lock. If the process exits,
  // the lock will be automatically released.
  //
  // If somebody else already holds the lock, finishes immediately
  // with a failure. I.e., this call does not wait for locks to go
  // away.
  //
  // May create the named file if it does not already exist.
  virtual Status LockFile(const std::string& filename, FileLock** lock) = 0;

  // Release the lock acquired by a previous successful call to
  // LockFile.
  // REQUIRES: lock was returned by a successful LockFile() call
  // REQUIRES: lock was not already been unlocked.
  virtual Status UnlockFile(FileLock* lock) = 0;

  // Arrange to run "(*function)(arg)" once in a background thread.
  //
  // "function" may run in an unspecified thread. Multiple functions
  // added to the same Env may run concurrently in different threads.
  // I.e.., the caller may not assume that background work items are
  // serialized.
  virtual void Schedule(std::function<void(void*)> func, void* arg) = 0;

  // Start a new thread, invoking "function(arg)" within the new thread.
  // When "function(arg)" returns, the thread will be destroyed.
  virtual void StartThread(std::function<void(void*)> func, void* arg) = 0;

  // *path is set to a temporary directory that can be used for testing.
  // It may or may not have just been created. The directory may or may
  // not differ between runs of the same process, but subsequent calls
  // will return the same directory.
  virtual Status GetTestDirectory(std::string* path) = 0;

  // Create and return a log file for storing informational messages.
  virtual Status NewLogger(const std::string& filename, Logger** result) = 0;

  // Returns the number of micro-seconds since some fixed point in time.
  /// Only useful for computing deltas of time.
  virtual uint64_t NowMicros() = 0;

  // Sleep/delay of the thread for the prescribed number of micro-seconds.
  virtual void SleepForMicroseconds(int micros) = 0;
};

// A file abstraction for reading sequentially through a file
class LSMDB_EXPORT SequentialFile : public noncopyable {
 public:
  SequentialFile() = default;

  virtual ~SequentialFile();

  // Read up to "n" bytes from the file. "scratch[0..n-1]" may be
  // written by this routine. Sets "*result" to the data that was
  // read (including if fewer than "n" bytes were successfully
  // read). May set "*result" to point to data in "scratch[0..n-1]",
  // so "scratch[0..n-1]" must be live when "*result" is used.
  // If an error was encountered, returns a non-OK status.
  //
  // REQUIRES: External synchronization.
  virtual Status Read(size_t n, Slice* result, char* scratch) = 0;

  // Skip "n" bytes from the file. This is guaranteed to be no
  // slower that reading the same data, but may be faster.
  //
  // If end of file is reached, skipping will stop at the end of
  // the file, and Skip will return OK.
  virtual Status Skip(uint64_t n) = 0;
};

// A file abstraction for randomly reading the contents of a file.
class LSMDB_EXPORT RandomAccessFile : public noncopyable{
 public:
  RandomAccessFile() = default;

  virtual ~RandomAccessFile();

  // Read up to "n" bytes from the file starting at "offset".
  // "scratch[0..n-1]" may be written by this routine. Sets
  // "*result" to the data that was read (including if fewer
  // than "n" bytes were successfully read). May set "*result"
  // to point at data in "scratch[0..n-1]", so "scratch[0..n-1]"
  // must be live when "*result" is used. If an error was
  // encountered, returns a non-OK status.
  //
  /// Safe for concurrent use by multiple threads.
  virtual Status Read(uint64_t offset, size_t n, Slice* result,
                      char* scratch) const = 0;
};

// A file abstraction for sequential writing. The implementation
// must provide buffering since callers may append small fragments
// at a time to the file.
class LSMDB_EXPORT WritableFile : public noncopyable {
 public:
  WritableFile() = default;

  virtual ~WritableFile();

  virtual Status Append(const Slice& data) = 0;
  virtual Status Close() = 0;
  virtual Status Flush() = 0;
  virtual Status Sync() = 0;
};

// An interface for writing log messages.
class LSMDB_EXPORT Logger : public noncopyable {
 public:
  Logger() = default;

  virtual ~Logger();

  // Write an entry to the log file with the specified format.
  virtual void Logv(const char* format, std::va_list ap) = 0;
};

// Identifies a locked file.
class LSMDB_EXPORT FileLock : public noncopyable {
 public:
  FileLock() = default;

  virtual ~FileLock();
};

// Log the specified data to *info_log if info_log is non-null.
void Log(Logger* info_log, const char* format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((__format__(__printf__, 2, 3)))
#endif
    ;

// A utility routine: write "data" to the named file.
LSMDB_EXPORT Status WriteStringToFile(Env* env, const Slice& data,
                                      const std::string& filename);

// A utility routine: read contents of named file into *data
LSMDB_EXPORT Status ReadFileToString(Env* env, const std::string& filename,
                                     std::string* data);

// An implementation of Env that forwards all calls to another Env.
// May be useful to clients who wish to override just part of the
// functionality of another Env.
class LSMDB_EXPORT EnvWapper : public Env {
public:
  // Initialize an EnvWrapper that delegates all calls to *t.
  explicit EnvWapper(Env* t) : target_(t) {}
  virtual ~EnvWapper() override;

  // Return the target to which this Env forwards all calls.
  Env* target() const { return target_; }

  // The following text is boilerplate that forwards all methods to target().
  Status NewSequentialFile(const std::string& f, SequentialFile** r) override {
    return target_->NewSequentialFile(f, r);
  }
  Status NewRandomAccessFile(const std::string& f,
                             RandomAccessFile** r) override {
    return target_->NewRandomAccessFile(f, r);
  }
  Status NewWritableFile(const std::string& f, WritableFile** r) override {
    return target_->NewWritableFile(f, r);
  }
  Status NewAppendableFile(const std::string& f, WritableFile** r) override {
    return target_->NewAppendableFile(f, r);
  }
  bool FileExists(const std::string& f) override {
    return target_->FileExists(f);
  }
  Status GetChildren(const std::string& dir,
                     std::vector<std::string>* r) override {
    return target_->GetChildren(dir, r);
  }
  Status RemoveFile(const std::string& f) override {
    return target_->RemoveFile(f);
  }
  Status CreateDir(const std::string& d) override {
    return target_->CreateDir(d);
  }
  Status RemoveDir(const std::string& d) override {
    return target_->RemoveDir(d);
  }
  Status GetFileSize(const std::string& f, uint64_t* s) override {
    return target_->GetFileSize(f, s);
  }
  Status RenameFile(const std::string& s, const std::string& t) override {
    return target_->RenameFile(s, t);
  }
  Status LockFile(const std::string& f, FileLock** l) override {
    return target_->LockFile(f, l);
  }
  Status UnlockFile(FileLock* l) override { return target_->UnlockFile(l); }
  void Schedule(std::function<void (void*)> f, void* a) override {
    return target_->Schedule(f, a);
  }
  void StartThread(std::function<void (void*)> f, void* a) override {
    return target_->StartThread(f, a);
  }
  Status GetTestDirectory(std::string* path) override {
    return target_->GetTestDirectory(path);
  }
  Status NewLogger(const std::string& fname, Logger** result) override {
    return target_->NewLogger(fname, result);
  }
  uint64_t NowMicros() override { return target_->NowMicros(); }
  void SleepForMicroseconds(int micros) override {
    target_->SleepForMicroseconds(micros);
  }

private:
  Env* target_;
};

}  // namespace lsmdb

// This workaround can be removed when leveldb::Env::DeleteFile is removed.
// Redefine DeleteFile if it was undefined earlier.
#if defined(_WIN32) && defined(LEVELDB_DELETEFILE_UNDEFINED)
#if defined(UNICODE)
#define DeleteFile DeleteFileW
#else
#define DeleteFile DeleteFileA
#endif  // defined(UNICODE)
#endif  // defined(_WIN32) && defined(LEVELDB_DELETEFILE_UNDEFINED)

#endif  // STORAGE_LSMDB_INCLUDE_ENV_H_
