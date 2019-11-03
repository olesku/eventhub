#ifndef EVENTHUB_CONFIG_HPP
#define EVENTHUB_CONFIG_HPP

#include <string>

namespace eventhub {
class EventhubConfig {
public:
  static EventhubConfig& getInstance() {
    static EventhubConfig instance;
    return instance;
  }

  const std::string getJWTSecret() {
    return "eventhub_secret";
  }

  unsigned int getPingInterval() {
    return 10;
  }

  const std::string getRedisPrefix() {
    return "eventhub";
  }
};
} // namespace eventhub

#define Config EventhubConfig::getInstance()

#endif
