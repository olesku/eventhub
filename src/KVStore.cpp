#include <memory>

#include "KVStore.hpp"
#include "Common.hpp"
#include "Config.hpp"

namespace eventhub {
  const std::string& KVStore::get(const std::string& key) const {
    const std::string value = _redis.connection()->get(REDIS_PREFIX(key)).value();
    LOG->info("KVStore read: {} = {}", REDIS_PREFIX(key), value);
    return std::move(value);
  }

  void KVStore::set(const std::string& key, const std::string& value, unsigned long ttl) const {
    if (ttl > 0) {
      _redis.connection()->setex(REDIS_PREFIX(key), ttl, value);
    } else {
      _redis.connection()->set(REDIS_PREFIX(key), value);
    }

    LOG->info("KVStore write: {} = {} TTL: {}", key, value, ttl);
  }

  void KVStore::del(const std::string& key) const {
    try {
      _redis.connection()->del(REDIS_PREFIX(key));
    } catch(...) {}
    LOG->info("KVStore erase: {}", REDIS_PREFIX(key));
  }
}