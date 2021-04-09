//
// Created by 刘文景 on 2021/4/9.
//

#ifndef STORAGE_LSMDB_HELPERS_MEMENV_MEMENV_H_
#define STORAGE_LSMDB_HELPERS_MEMENV_MEMENV_H_

#include "lsmdb/export.h"

namespace lsmdb {

class Env;

// Returns a new environment that stores its data in memory and delegates
// all non-file-storage tasks to base_env. The caller must delete the
// result when it is no longer needed.
// *base_env must remain live while the result is in use.
LSMDB_EXPORT Env* NewMemEnv(Env* base_env);

} // namespace lsmdb

#endif //STORAGE_LSMDB_HELPERS_MEMENV_MEMENV_H_
