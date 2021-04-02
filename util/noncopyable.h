//
// Created by 刘文景 on 2021/4/2.
//

#ifndef STORAGE_LSMDB_UTIL_NONCOPYABLE_H_
#define STORAGE_LSMDB_UTIL_NONCOPYABLE_H_

namespace lsmdb {

class noncopyable{
public:
  noncopyable(const noncopyable&) = delete;
  void operator=(const noncopyable&) = delete;
protected:
  noncopyable() = default;
  ~noncopyable() = default;
};

}

#endif //STORAGE_LSMDB_UTIL_NONCOPYABLE_H_
