//
// Created by 刘文景 on 2021/3/25.
//

#ifndef STORAGE_LSMDB_HASH_H
#define STORAGE_LSMDB_HASH_H

#include <cstddef>
#include <cstdint>

namespace lsmdb {

uint32_t Hash(const char* data, size_t n, uint32_t seed);

}  // namespace lsmdb

#endif  // STORAGE_LSMDB_HASH_H
