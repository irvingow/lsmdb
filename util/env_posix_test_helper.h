//
// Created by 刘文景 on 2021/3/30.
//

#ifndef STORAGE_LSMDB_UTIL_ENV_POSIX_TEST_HELPER_H_
#define STORAGE_LSMDB_UTIL_ENV_POSIX_TEST_HELPER_H_

namespace lsmdb {

class EnvPosixTest;

// A helper for the POSIX Env to facilitate testing.
class EnvPosixTestHelper {
private:
  friend class EnvPosixTest;

  // Set the maximum helper of read-only files that will be opened.
  // Must be called before creating an Env.
  static void SetReadOnlyFDLimit(int limit);

  // Set the maximum number of read-only files that will be mapped
  // via mmap.
  // Must be called before creating an Env.
  static void SetReadOnlyMMapLimit(int limit);
};

} // namespace lsmdb

#endif //STORAGE_LSMDB_UTIL_ENV_POSIX_TEST_HELPER_H_
