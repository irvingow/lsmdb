//
// Created by 刘文景 on 2021/3/25.
//

#include "lsmdb/cache.h"

#include <vector>

#include "gtest/gtest.h"
#include "util/coding.h"

namespace lsmdb {

// Conversions between numeric keys/value and the types expected by Cache.
static std::string EncodeKey(int k) {
  std::string result;
  PutFixed32(&result, k);
  return result;
}

static int DecodeKey(const Slice& k) {
  assert(k.size() == 4);
  return DecodeFixed32(k.data());
}

static void* EncodeValue(uintptr_t v) { return reinterpret_cast<void*>(v); }
static int DecodeValue(void* v) { return reinterpret_cast<uintptr_t>(v); }

// static void Deleter(std::vector<int>* deleted_keys_,
//                    std::vector<int>* deleted_values_, const Slice& key,
//                    void* v) {
//  deleted_keys_->push_back(DecodeKey(key));
//  deleted_values_->push_back(DecodeValue(v));
//}

class CacheTest : public testing::Test {
 public:
  static void Deleter(const Slice& key, void* v) {
    current_->deleted_keys_.push_back(DecodeKey(key));
    current_->deleted_values_.push_back(DecodeValue(v));
  }

  static constexpr int kCacheSize = 1000;
  std::vector<int> deleted_keys_;
  std::vector<int> deleted_values_;
  std::shared_ptr<Cache> cache_;

  CacheTest() : cache_(NewLRUCache(kCacheSize)) { current_ = this; }

  void SetUp() override {
    cache_ = NewLRUCache(kCacheSize);
    current_ = this;
  }

  void TearDown() override {
    cache_.reset();
  }

  ~CacheTest() override = default;

  int Lookup(int key) {
    auto handle = cache_->Lookup(EncodeKey(key));
    const int r = (handle == nullptr) ? -1 : DecodeValue(cache_->Value(handle));
    if (handle != nullptr) {
      cache_->Release(handle);
    }
    return r;
  }

  void Insert(int key, int value, int charge = 1) {
    // 注意这里直接释放了返回的handle，因此handle只存在于lru_中并且refs=1
    cache_->Release(cache_->Insert(
        EncodeKey(key), EncodeValue(value), charge,
        CacheTest::Deleter));
  }

  Cache::Handle* InsertAndReturnHandle(int key, int value, int charge = 1) {
    return cache_->Insert(
        EncodeKey(key), EncodeValue(value), charge,
        CacheTest::Deleter);
  }

  void Erase(int key) { cache_->Erase(EncodeKey(key)); }
  static CacheTest* current_;
};

CacheTest* CacheTest::current_;

/// some notes about Cache:
/// 1. if handle is in hashtable, then handle must be in lru_ or in_cache_ list.
/// 2. if handle is in hashtable, then in_cache_ is true.
/// 3. when we create a new handle, the handle initially is in in_cache_ list,
///    if we call Release on the returned handle, then the handle will be moved
///    from in_cache_ to lru_ but still in hashtable(with refs = 1 and may be
///    eliminated when capacity is full).
/// 4. when we call Erase on an existed handle, the handle will be removed from
///    hashtable(which means it's removed from lru_ or in_cache_ list, if the
///    handle is still held by clients, the handle will not be deleted until
///    clients call Release on the handle.
/// 5. if a handle is in in_cache_ list(which means its refs = 2), it will not
///    be eliminated until Erase is called or Release is called.

 TEST_F(CacheTest, Simple) {
  ASSERT_EQ(-1, Lookup(100));
  Insert(100, 101);
  ASSERT_EQ(101, Lookup(100));
  Insert(100, 201);
  ASSERT_EQ(201, Lookup(100));
  Erase(100);
  ASSERT_EQ(-1, Lookup(100));
  Insert(200, 201);
  ASSERT_EQ(201, Lookup(200));
  Erase(200);
  ASSERT_EQ(-1, Lookup(200));
  // save the handle
  auto h = InsertAndReturnHandle(100, 101);
  ASSERT_EQ(101, Lookup(100));
  Erase(100);
  // After Erase(100), key:100 is eliminated
  // from cache, but still one handle is alive.
  ASSERT_EQ(-1, Lookup(100));
  // there are three deleted keys and values.
  // key:100 value:101
  // key:100 value:201
  // key:200 value:201
  ASSERT_EQ(3, deleted_keys_.size());
  ASSERT_EQ(101, deleted_values_[0]);
  ASSERT_EQ(201, deleted_values_[1]);
  ASSERT_EQ(201, deleted_values_[2]);
  // Release the handle.
  cache_->Release(h);
  // key:100 value:101
  // is added to deleted_keys_ and deleted_values_.
  ASSERT_EQ(4, deleted_keys_.size());
  ASSERT_EQ(101, deleted_values_[3]);
}

TEST_F(CacheTest, HitAndMiss) {
  ASSERT_EQ(-1, Lookup(100));

  Insert(100, 101);
  ASSERT_EQ(101, Lookup(100));
  ASSERT_EQ(-1, Lookup(200));
  ASSERT_EQ(-1, Lookup(300));

  Insert(200, 201);
  ASSERT_EQ(101, Lookup(100));
  ASSERT_EQ(201, Lookup(200));
  ASSERT_EQ(-1, Lookup(300));

  Insert(100, 102);
  ASSERT_EQ(102, Lookup(100));
  ASSERT_EQ(201, Lookup(200));
  ASSERT_EQ(-1, Lookup(300));

  // key:100 value:101 is erased, so deleter is called.
  ASSERT_EQ(1, deleted_keys_.size());
  ASSERT_EQ(100, deleted_keys_[0]);
  ASSERT_EQ(101, deleted_values_[0]);
}

TEST_F(CacheTest, Erase) {
  Erase(200);
  ASSERT_EQ(0, deleted_keys_.size());
  Insert(400, 401);
  ASSERT_EQ(401, Lookup(400));

  Insert(100, 101);
  Insert(200, 201);
  Erase(100);
  ASSERT_EQ(-1, Lookup(100));
  ASSERT_EQ(201, Lookup(200));
  ASSERT_EQ(1, deleted_keys_.size());
  ASSERT_EQ(100, deleted_keys_[0]);
  ASSERT_EQ(101, deleted_values_[0]);

  Erase(100);
  ASSERT_EQ(-1, Lookup(100));
  ASSERT_EQ(201, Lookup(200));
  ASSERT_EQ(1, deleted_keys_.size());
}

TEST_F(CacheTest, EntriesArePinned) {
  Insert(100, 101);
  // notice that after we call @func Lookup and save h1
  // the h1 handle is moved from lru_ back to in_cache_
  auto h1 = cache_->Lookup(EncodeKey(100));
  ASSERT_EQ(101, DecodeValue(cache_->Value(h1)));

  // notice h1 is removed from hashtable(in_cache_ is false
  // and h1 is not in lru_ neither in in_cache_), only a
  // handle h1 is alive.
  Insert(100, 102);
  auto h2 = cache_->Lookup(EncodeKey(100));
  ASSERT_EQ(102, DecodeValue(cache_->Value(h2)));
  ASSERT_EQ(0, deleted_keys_.size());

  cache_->Release(h1);
  ASSERT_EQ(1, deleted_keys_.size());
  ASSERT_EQ(100, deleted_keys_[0]);
  ASSERT_EQ(101, deleted_values_[0]);

  // same as h1, h2 handle is removed from hashtable,
  // only a handle h2 is alive.
  Erase(100);
  ASSERT_EQ(-1, Lookup(100));
  ASSERT_EQ(1, deleted_keys_.size());

  cache_->Release(h2);
  ASSERT_EQ(2, deleted_keys_.size());
  ASSERT_EQ(100, deleted_keys_[1]);
  ASSERT_EQ(102, deleted_values_[1]);
}

TEST_F(CacheTest, EvictionPolicy) {
  Insert(100, 101);
  Insert(200, 201);
  Insert(300, 301);
  auto h = cache_->Lookup(EncodeKey(300));

  // Frequently used entry must be kept around,
  // as must things that are still in use.
  for (int i = 0; i < kCacheSize + 100; ++i) {
    Insert(1000 + i, 2000 + i);
    ASSERT_EQ(2000 + i, Lookup(1000 + i));
    ASSERT_EQ(101, Lookup(100));
  }
  ASSERT_EQ(101, Lookup(100));
  ASSERT_EQ(-1, Lookup(200));
  ASSERT_EQ(301, Lookup(300));
  cache_->Release(h);
}

TEST_F(CacheTest, UseExceedsCacheSize) {
  // Overfill the cache, keeping handles on all inserted entries.
  std::vector<Cache::Handle*> h(kCacheSize + 100, nullptr);
  for (int i = 0; i < kCacheSize + 100; ++i) {
    h[i] = InsertAndReturnHandle(1000 + i, 2000 + i);
  }
  // Check that all the entries can be found in the cache.
  for (int i = 0; i < h.size(); ++i) {
    ASSERT_EQ(2000 + i, Lookup(1000 + i));
  }
  for (auto* handle : h) {
    cache_->Release(handle);
  }
}

TEST_F(CacheTest, HeavyEntries) {
  // Add a bunch of light and heavy entries and then count the combined
  // size of items still in the cache, which must be approximately the
  // same as the total capacity.
  const int kLight = 1;
  const int kHeavy = 10;
  int added = 0;
  int index = 0;
  while (added < 2 * kCacheSize) {
    const int weight = (index & 1) ? kLight : kHeavy;
    Insert(index, 1000 + index, weight);
    added += weight;
    index++;
  }
  int cached_weight = 0;
  for (int i = 0; i < index; ++i) {
    const int weight = (i & 1 ? kLight : kHeavy);
    int r = Lookup(i);
    if (r >= 0) {
      cached_weight += weight;
      ASSERT_EQ(1000 + i, r);
    }
  }
  ASSERT_LE(cached_weight, kCacheSize + kCacheSize / 10);
}

TEST_F(CacheTest, NewId) {
  uint64_t a = cache_->NewId();
  uint64_t b = cache_->NewId();
  ASSERT_NE(a, b);
}

TEST_F(CacheTest, Prune) {
  Insert(1, 100);
  Insert(2, 100);
  // now key:1 is in in_cache_ with refs = 2.
  // key:2 is in lru_ with refs = 1.

  auto handle = cache_->Lookup(EncodeKey(1));
  ASSERT_TRUE(handle);
  cache_->Prune();
  cache_->Release(handle);

  ASSERT_EQ(100, Lookup(1));
  ASSERT_EQ(-1, Lookup(2));
}

TEST_F(CacheTest, ZeroSizeCache) {
  cache_ = NewLRUCache(0);

  Insert(1, 100);
  // becase cache is banned, so now key:1 is removed.
  ASSERT_EQ(-1, Lookup(1));
}

}  // namespace lsmdb

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
