#include <string>
#include <chrono>

#include "KVStore.hpp"
#include "Common.hpp"
#include "Config.hpp"

namespace eventhub {
  const std::string KVStore::_prefix_key(const std::string& key) const {
    return _prefix.empty() ? std::string(_prefix + ":kv:" + key)
                           : std::string("kv:" + key);
  }

  bool KVStore::is_enabled() {
    return config().get<bool>("enable_kvstore");
  }

  const std::string KVStore::get(const std::string& key) const {
    const std::string value = _redis.connection()->get(_prefix_key(key)).value();
    return value;
  }

  bool KVStore::set(const std::string& key, const std::string& value, unsigned long ttl) const {
    if (ttl > 0) {
      return _redis.connection()->set(_prefix_key(key), value, std::chrono::seconds(ttl));
    } else {
      return _redis.connection()->set(_prefix_key(key), value);
    }
  }

  long long KVStore::del(const std::string& key) const {
    long long ret = 0;

    try {
      ret = _redis.connection()->del(_prefix_key(key));
    } catch(...) {}

    return ret;
  }
}