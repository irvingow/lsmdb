//
// Created by 刘文景 on 2021/3/25.
//

#include "lsmdb/cache.h"

#include <port/port.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "port/port.h"
#include "port/thread_annotations.h"
#include "util/hash.h"
#include "util/mutexlock.h"

namespace lsmdb {

Cache::~Cache() {}

namespace {

// LRU cache implementation
//
// Cache entries have an "in_cache" boolean indicating whether the cache has a
// reference on the entry. The only ways that this can become false without the
// entry being passed to its "deleter" are via Erase(), via Insert() when an
// element with a duplicate key is inserted, or on destruction of the cache.
//
// The cache keeps two linked lists of items in the cache. All items in the
// cache are in one list or the other, and NEVER the both. Items still
// referenced by clients but erased from the cache are in neither list. The
// lists are:
// - in-use: contains the items currently referenced by clients, in no
//   particular order. (This list is used for invariant checking. If we remove
//   the check, elements that would be otherwise be on this list could be left
//   as disconnected singleton lists.
// - LRU: contains the items not currently referenced by clients, in LRU order
//   Elements are moved between these lists by the Ref() and Unref() methods,
//   when they detect an element in the cache acquiring or losing its only
//   external reference.

// An entry is a variable length heap-allocated structure. Entries
// are kept in a circular doubly linked list ordered by access time.
struct LRUHandle {
  LRUHandle()
      : value(nullptr),
        deleter(nullptr),
        next_hash(nullptr),
        next(nullptr),
        prev(nullptr),
        charge(0),
        key_length(0),
        in_cache(false),
        refs(0),
        hash(0),
        key_data(nullptr) {}
  ~LRUHandle() {
    free(key_data);
  }
  // 缓存的内容
  void* value;
  // 回调函数
  std::function<void(const Slice&, void* value)> deleter;
  //
  LRUHandle* next_hash;
  //
  LRUHandle* next;
  //
  LRUHandle* prev;
  size_t charge;
  size_t key_length;  // Length of key
  bool in_cache;      // Whether entry is in the cache
  uint32_t refs;      // References, including cache reference, if present
  uint32_t hash;      // Hash of key(); used for fast sharding and comparsions
  char *key_data;   // pointer to the key_data

  Slice key() const {
    // next is only equal to this if the LRU handle is the list head of an
    // empty list. List heads never have meaningful keys.
    assert(next != this);

    return Slice(key_data, key_length);
  }
};

// 这里使用了自己封装的简单的hashtable，在一些场景下性能比自带的实现更好
class HandleTable {
 public:
  HandleTable() : length_(0), elems_(0), table_(nullptr) { Resize(); }
  ~HandleTable() { delete[] table_; }

  LRUHandle* Lookup(const Slice& key, uint32_t hash) {
    return *FindPointer(key, hash);
  }

  LRUHandle* Insert(LRUHandle* h) {
    auto ptr = FindPointer(h->key(), h->hash);
    auto old = *ptr;
    // 我们只需要修改一下h->next_hash使其指向old->next_hash
    // 原先list_里面指向old的上一个handle(prev->next_hash)
    // 在我们进行*ptr = h会指向h
    h->next_hash = (old == nullptr) ? nullptr : old->next_hash;
    // use h to replace old
    *ptr = h;
    if (old == nullptr) {
      ++elems_;
      if (elems_ > length_) {
        // Since each cache entry is fairly large, we aim
        // for a small average linked list length (<= 1).
        Resize();
      }
    }
    return old;
  }

  LRUHandle* Remove(const Slice& key, uint32_t hash) {
    auto ptr = FindPointer(key, hash);
    auto result = *ptr;
    if (result != nullptr) {
      *ptr = result->next_hash;
      --elems_;
    }
    return result;
  }

 private:
  // The table consists of an array of buckets where each bucket is
  // a linked list of cache entries that hash into the bucket.
  uint32_t length_;
  uint32_t elems_;
  LRUHandle** table_;

  LRUHandle** FindPointer(const Slice& key, uint32_t hash) {
    // 注意这里返回二级指针的原因
    // list_[hash & (length_ - 1)]是一个分配在堆上的指针
    // 情况1、list_[hash & (length_ - 1)] == NULL时，新添加的节点需添加在其后，
    // 因此list_[hash & (length_ - 1)] = 新添加的节点内存地址
    // 情况2、list_[hash & (length_ - 1)] != NULL时，通常链表插入时需修改前一个
    // 节点的next_hash,因此需要前一个节点的地址，函数返回为二级指针，
    // 就是前一个节点next_hash指针的自身地址因此一行 *ptr =
    // h;就完成了旧指针的踢出 及新节点的加入
    LRUHandle** ptr = &table_[hash & (length_ - 1)];
    while (*ptr != nullptr && ((*ptr)->hash != hash || key != (*ptr)->key())) {
      ptr = &(*ptr)->next_hash;
    }
    return ptr;
  }

  void Resize() {
    uint32_t new_length = 4;
    while (new_length < elems_) {
      new_length *= 2;
    }
    auto new_table = new LRUHandle*[new_length];
    memset(new_table, 0, sizeof(new_table[0]) * new_length);
    uint32_t count = 0;
    for (uint32_t i = 0; i < length_; ++i) {
      auto h = table_[i];
      // 一次处理完当前hash值相同的所有key
      while (h != nullptr) {
        auto next = h->next_hash;
        auto hash = h->hash;
        // 找到当前handle在新的table中所处的位置
        LRUHandle** ptr = &new_table[hash & (new_length - 1)];
        // 将当前handle和之前已经存在的handle连接起来
        h->next_hash = *ptr;
        *ptr = h;
        h = next;
        count++;
      }
    }
    assert(elems_ == count);
    delete[] table_;
    table_ = new_table;
    length_ = new_length;
  }
};

// A single shard of sharded cache.
class LRUCache {
 public:
  LRUCache();
  ~LRUCache();

  // Separate from constructor so caller can easily make an array of LRUCache
  void SetCapacity(size_t capacity) { capacity_ = capacity; }

  // Like Cache methods, but with an extra "hash" parameter.
  Cache::Handle* Insert(const Slice& key, uint32_t hash, void* value,
                        size_t charge,
  std::function<void(const Slice& key, void* value)>);
  Cache::Handle* Lookup(const Slice& key, uint32_t hash);
  void Release(Cache::Handle* handle);
  void Erase(const Slice& key, uint32_t hash);
  void Prune();
  size_t TotalCharge() const {
    MutexLock lock(&mutex_);
    return usage_;
  }

 private:
  static void LRU_Remove(LRUHandle* e);
  static void LRU_Append(LRUHandle* list, LRUHandle* e);
  void Ref(LRUHandle* e);
  void Unref(LRUHandle* e);
  bool FinishErase(LRUHandle* e) EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // Initialized before use.
  size_t capacity_;

  // mutex_ protects the following state.
  mutable port::Mutex mutex_;
  // current cache usage.
  size_t usage_ GUARDED_BY(mutex_);

  /// Dummy head of LRU list
  /// lru.prev is newest entry, lru.next is oldest entry.
  /// Entries have refs == 1 and in_cache = true;
  LRUHandle lru_ GUARDED_BY(mutex_);

  /// Dummy head of in-use list.
  /// Entries are in use by clients, and have refs >= 2 and in_cache == true;
  LRUHandle in_use_ GUARDED_BY(mutex_);

  HandleTable table_ GUARDED_BY(mutex_);
};

LRUCache::LRUCache() : capacity_(0), usage_(0) {
  // Make empty circular linked lists.
  lru_.next = &lru_;
  lru_.prev = &lru_;
  in_use_.next = &in_use_;
  in_use_.prev = &in_use_;
}

LRUCache::~LRUCache() {
  // Error if caller has an unreleased handle
  assert(in_use_.next == &in_use_);
  for (auto e = lru_.next; e != &lru_;) {
    auto next = e->next;
    assert(e->in_cache);
    e->in_cache = false;
    // Invariant of lru_ list.
    assert(e->refs == 1);
    Unref(e);
    e = next;
  }
}

void LRUCache::Ref(lsmdb::LRUHandle* e) {
  // If on lru_ list, move to in_use list.
  if (e->refs == 1 && e->in_cache) {
    LRU_Remove(e);
    LRU_Append(&in_use_, e);
  }
  e->refs++;
}

void LRUCache::Unref(lsmdb::LRUHandle* e) {
  assert(e->refs > 0);
  e->refs--;
  if (e->refs == 0) {  // Deallocate.
    assert(!e->in_cache);
    (e->deleter)(e->key(), e->value);
    delete e;
  } else if (e->in_cache && e->refs == 1) {
    // No longer in use; move to lru_ list.
    LRU_Remove(e);
    LRU_Append(&lru_, e);
  }
}

void LRUCache::LRU_Remove(lsmdb::LRUHandle* e) {
  // 将e从它当前所在的链表上移走
  e->next->prev = e->prev;
  e->prev->next = e->next;
}

void LRUCache::LRU_Append(lsmdb::LRUHandle* list, lsmdb::LRUHandle* e) {
  // Make "e" newest entry by inserting just before *list
  // 这里是实现LRU的关键，我们将e插到list前面(prev)，这样在删除的时候从
  // lru_.next开始删除节点，lru_.next代表这个节点是最老的
  e->next = list;
  e->prev = list->prev;
  e->prev->next = e;
  e->next->prev = e;
}

Cache::Handle* LRUCache::Lookup(const Slice& key, uint32_t hash) {
  MutexLock lock(&mutex_);
  auto e = table_.Lookup(key, hash);
  if (e != nullptr) {
    Ref(e);
  }
  return reinterpret_cast<Cache::Handle*>(e);
}

void LRUCache::Release(Cache::Handle* handle) {
  MutexLock lock(&mutex_);
  Unref(reinterpret_cast<LRUHandle*>(handle));
}

Cache::Handle* LRUCache::Insert(
    const Slice& key, uint32_t hash, void* value, size_t charge,
  std::function<void(const Slice&, void*)> deleter) {
    MutexLock lock(&mutex_);

  // notice we should use new instead of malloc
  // LRUHandle.deleter is std::function
  // so we can't use malloc to allocate memory
  auto e = new LRUHandle;
  e->value = value;
  e->deleter = std::move(deleter);
  e->charge = charge;
  e->key_length = key.size();
  e->hash = hash;
  e->in_cache = false;
  e->refs = 1;  // for the returned handle.
  e->key_data = reinterpret_cast<char *>(malloc(key.size()));
  std::memcpy(e->key_data, key.data(), key.size());

  if (capacity_ > 0) {
    e->refs++;  // for the cache's reference.
    e->in_cache = true;
    LRU_Append(&in_use_, e);
    usage_ += charge;
    FinishErase(table_.Insert(e));
  } else {  // don't cache. (capacity_ == 0 is supported and turns off
            // cacheing.)
            // next is read by key() in an assert, so it must be initialized.
            // e->next is initialized in default constructor.
            // e->next = nullptr;
  }
  while (usage_ > capacity_ && lru_.next != &lru_) {
    // 缓存淘汰，当到达容量上限之后，淘汰访问最不频繁的节点即lru_的next
    auto old = lru_.next;
    assert(old->refs == 1);
    bool erased = FinishErase(table_.Remove(old->key(), old->hash));
    if (!erased) {  // to avoid unused variable when compiled NDEBUG
      assert(erased);
    }
  }

  // 将插入的handle强制类型转换为Cache::Handle返回
  return reinterpret_cast<Cache::Handle*>(e);
}

// If e != nullptr, finish removing *e from the cache. it has already
// been removed from the hash table. Return whether e != nullptr
// 注意这个函数在被调用的场景都是e已经从hashtable中被移除了
bool LRUCache::FinishErase(lsmdb::LRUHandle* e) {
  if (e != nullptr) {
    // 注意无论e是在lru_或者cache_中
    // e->in_cache都是true
    assert(e->in_cache);
    // 将e从当前所在的双向链表中删除
    LRU_Remove(e);
    // 因为e已经从hashtable中被删除了
    e->in_cache = false;
    usage_ -= e->charge;
    Unref(e);
  }
  return e != nullptr;
}

void LRUCache::Erase(const Slice& key, uint32_t hash) {
  MutexLock lock(&mutex_);
  FinishErase(table_.Remove(key, hash));
}

void LRUCache::Prune() {
  MutexLock lock(&mutex_);
  // 仅仅针对lru_链表的中handle
  while (lru_.next != &lru_) {
    auto e = lru_.next;
    assert(e->refs == 1);
    bool erased = FinishErase(table_.Remove(e->key(), e->hash));
    if (!erased) {  // to avoid unused variable when compiled NDEBUG
      assert(erased);
    }
  }
}

const int kNumShardBits = 4;
const int kNumShards = 1 << kNumShardBits;

// LRUCache的接口都会加锁，为了减少锁竞争以及更高的缓存命中率
// 可以定义多个LRUCache，分别处理不同hash取模后的缓存处理
class ShardedLRUCache : public Cache {
 public:
  explicit ShardedLRUCache(size_t capacity) : last_id_(0) {
    const size_t per_shard = (capacity + (kNumShards - 1)) / kNumShards;
    for (int s = 0; s < kNumShards; ++s) {
      shard_[s].SetCapacity(per_shard);
    }
  }
  ~ShardedLRUCache() override = default;
  Handle* Insert(
      const Slice& key, void* value, size_t charge,
      std::function<void(const Slice& key, void* value)> deleter) override {
    const uint32_t hash = HashSlice(key);
    return shard_[Shard(hash)].Insert(key, hash, value, charge, deleter);
  }
  Handle* Lookup(const Slice& key) override {
    const uint32_t hash = HashSlice(key);
    return shard_[Shard(hash)].Lookup(key, hash);
  }
  void Release(Handle* handle) override {
    auto h = reinterpret_cast<LRUHandle*>(handle);
    shard_[Shard(h->hash)].Release(handle);
  }
  void Erase(const Slice& key) override {
    const uint32_t hash = HashSlice(key);
    shard_[Shard(hash)].Erase(key, hash);
  }
  void* Value(Handle* handle) override {
    return reinterpret_cast<LRUHandle*>(handle)->value;
  }
  uint64_t NewId() override {
    MutexLock lock(&id_mutex_);
    return ++(last_id_);
  }
  void Prune() override {
    for (int i = 0; i < kNumShards; ++i) {
      shard_[i].Prune();
    }
  }

  size_t TotalCharge() const override {
    size_t total = 0;
    for (int i = 0; i < kNumShards; ++i) {
      total += shard_[i].TotalCharge();
    }
    return total;
  }

 private:
  LRUCache shard_[kNumShards];
  port::Mutex id_mutex_;
  uint64_t last_id_;

  static inline uint32_t HashSlice(const Slice& s) {
    return Hash(s.data(), s.size(), 0);
  }

  static uint32_t Shard(uint32_t hash) { return hash >> (32 - kNumShardBits); }
};

}  // end anonymous namespace

std::shared_ptr<Cache> NewLRUCache(size_t capacity) {
  return std::make_shared<ShardedLRUCache>(capacity);
}

}  // namespace lsmdb
