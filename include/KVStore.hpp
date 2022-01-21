#pragma once

#include <memory>
#include "EventhubBase.hpp"
#include "Redis.hpp"

namespace eventhub {
  class KVStore final : public EventhubBase {
    private:
      std::string _prefix;
      Redis& _redis;

      const std::string _prefix_key(const std::string& key) const;

    public:
      KVStore(Config& cfg, Redis& redis) :
        EventhubBase(cfg),
        _redis(redis) {
          _prefix =  config().get<std::string>("redis_prefix");
        }

      const std::string get(const std::string& key) const;
      bool set(const std::string& key, const std::string& value, unsigned long ttl = 0) const;
      long long del(const std::string& key) const;
  };
}
