//
// Created by 刘文景 on 2021/3/26.
//

#ifndef STORAGE_LSMDB_PORT_PORT_STDCXX_H_
#define STORAGE_LSMDB_PORT_PORT_STDCXX_H_

// port/port_config.h availability is automatically detected via __has_include
// in newer compilers. If LSMDB_HAS_PORT_CONFIG_H is defined, it overrides the
// configuration detection.
#if defined(LSMDB_HAS_PORT_CONFIG_H)

#if LSMDB_HAS_PORT_CONFIG_H
#include "port/port_config.h"
#endif  // LSMDB_HAS_PORT_CONFIG_H

#elif defined(__has_include)

#if __has_include("port/port_config.h")
#include "port/port_config.h"
#endif  // __has_include("port/port_config.h")

#endif  // defined(LSMDB_HAS_PORT_CONFIG_H)

#if HAVE_CRC32C
#include <crc32c/crc32c.h>
#endif  // HAVE_CRC32C
#if HAVE_SNAPPY
#include <snappy.h>
#endif  // HAVE_SNAPPY

#include <cassert>
#include <condition_variable>  // NOLINT
#include <cstddef>
#include <cstdint>
#include <mutex>  // NOLINT
#include <string>

#include "port/thread_annotations.h"
#include "util/noncopyable.h"

namespace lsmdb {
namespace port {

class CondVar;

// Thinly wraps std::mutex.
class LOCKABLE Mutex : public noncopyable {
    public:
    Mutex() = default;
    ~Mutex() = default;

    void Lock() EXCLUSIVE_LOCK_FUNCTION() { mu_.lock(); }
    void Unlock() UNLOCK_FUNCTION() { mu_.unlock(); }
    void AssertHeld() ASSERT_EXCLUSIVE_LOCK() {}

    private:
    friend class CondVar;
    std::mutex mu_;
};

// Thinly wraps std::condition_variable.
class CondVar : public noncopyable {
public:
    explicit CondVar(Mutex* mu) : mu_(mu) { assert(mu != nullptr); }
    ~CondVar() = default;

    void Wait() {
        std::unique_lock<std::mutex> lock(mu_->mu_, std::adopt_lock);
        cv_.wait(lock);
        lock.release();
    }
    void Signal() { cv_.notify_one(); }
    void SignalAll() { cv_.notify_all(); }

private:
    std::condition_variable cv_;
    Mutex* const mu_;
};

inline bool Snappy_Compress(const char* input, size_t length,
                            std::string* output) {
#if HAVE_SNAPPY
    output->resize(snappy::MaxCompressedLength(length));
  size_t outlen;
  snappy::RawCompress(input, length, &(*output)[0], &outlen);
  output->resize(outlen);
  return true;
#else
    // Silence compiler warnings about unused arguments.
    (void)input;
    (void)length;
    (void)output;
#endif  // HAVE_SNAPPY

    return false;
}

inline bool Snappy_GetUncompressedLength(const char* input, size_t length,
                                         size_t* result) {
#if HAVE_SNAPPY
    return snappy::GetUncompressedLength(input, length, result);
#else
    // Silence compiler warnings about unused arguments.
    (void)input;
    (void)length;
    (void)result;
    return false;
#endif  // HAVE_SNAPPY
}

inline bool Snappy_Uncompress(const char* input, size_t length, char* output) {
#if HAVE_SNAPPY
    return snappy::RawUncompress(input, length, output);
#else
    // Silence compiler warnings about unused arguments.
    (void)input;
    (void)length;
    (void)output;
    return false;
#endif  // HAVE_SNAPPY
}

inline bool GetHeapProfile(void (*func)(void*, const char*, int), void* arg) {
    // Silence compiler warnings about unused arguments.
    (void)func;
    (void)arg;
    return false;
}

inline uint32_t AcceleratedCRC32C(uint32_t crc, const char* buf, size_t size) {
#if HAVE_CRC32C
    return ::crc32c::Extend(crc, reinterpret_cast<const uint8_t*>(buf), size);
#else
    // Silence compiler warnings about unused arguments.
    (void)crc;
    (void)buf;
    (void)size;
    return 0;
#endif  // HAVE_CRC32C
}

}  // namespace port
}  // namespace lsmdb

#endif  // STORAGE_LSMDB_PORT_PORT_STDCXX_H_
