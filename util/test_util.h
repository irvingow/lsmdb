//
// Created by 刘文景 on 2021/3/29.
//

#ifndef STORAGE_LSMDB_UTIL_TEST_UTIL_H_
#define STORAGE_LSMDB_UTIL_TEST_UTIL_H_

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "helpers/memenv/memenv.h"
#include "lsmdb/env.h"
#include "lsmdb/slice.h"
#include "util/random.h"

namespace lsmdb {

namespace test {

MATCHER(IsOK, "") { return arg.ok(); }

// Macros for testing the results of functions that return lsmdb::Status
// or util::StatusOr<T> (for any type T).
#define EXPECT_LSMDB_OK(expression) EXPECT_THAT(expression, lsmdb::test::IsOK())
#define ASSERT_LSMDB_OK(expression) EXPECT_THAT(expression, lsmdb::test::IsOK())

// Returns the random seed used at the start of the current test run.
inline int RandomSeed() {
  return testing::UnitTest::GetInstance()->random_seed();
}

// Store in *dst a random string of length "len" and return a Slice that
// references the generated data.
Slice RandomString(Random* rnd, int len);

// Return a random key with the specified length that may contain
// interesting characters (e.g. \x00, \xff, etc.).
std::string RandomKey(Random* rnd, int len);

// Store in *dst a string of length "len" that will compress to
// "N*compressed_fraction" bytes and return a Slice that references
// the generated data.
Slice CompressibleString(Random* rnd, double compressed_fraction, size_t len,
                         std::string* dst);

class ErrorEnv : public EnvWapper {
 public:
  bool writable_file_error_;
  int num_writable_file_errors_;

  ErrorEnv()
      : EnvWapper(NewMemEnv(Env::Default())),
        writable_file_error_(false),
        num_writable_file_errors_(0) {}

  Status NewWritableFile(const std::string& filename,
                         WritableFile** result) override {
    if (writable_file_error_) {
      ++num_writable_file_errors_;
      *result = nullptr;
      return Status::IOError(filename, "fake error");
    }
    return target()->NewWritableFile(filename, result);
  }

  Status NewAppendableFile(const std::string& filename,
      WritableFile** result) override {
    if (writable_file_error_) {
      ++num_writable_file_errors_;
      *result = nullptr;
      return Status::IOError(filename, "fake error");
    }
    return target()->NewAppendableFile(filename, result);
  }
};

}  // namespace test

}  // namespace lsmdb

#endif  // STORAGE_LSMDB_UTIL_TEST_UTIL_H_
