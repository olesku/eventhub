#pragma once

#include <memory>
#include <string>
#include <vector>
#include <exception>

#include "Forward.hpp"
#include "EventhubBase.hpp"
#include "jwt/jwt.hpp"

namespace eventhub {

typedef struct {
  std::string topic;
  unsigned long interval;
  unsigned long max;
} rlimit_config_t;

struct NoRateLimitForTopic : public std::exception {
  const char* what() const throw() {
    return "Token has no rate limits defined";
  }
};

class RateLimitConfig final {
  private:
    std::vector<rlimit_config_t> _limitConfigs;

  public:
    bool loadFromJSON(const nlohmann::json::array_t& config);
    const rlimit_config_t getRateLimitForTopic(const std::string& topic);
};

class AccessController final : public EventhubBase {
private:
  bool _token_loaded;
  std::string _jwt_secret;
  std::string _subject;
  jwt::jwt_object _token;
  std::vector<std::string> _publish_acl;
  std::vector<std::string> _subscribe_acl;
  RateLimitConfig _rlimit;

public:
  AccessController(Config &cfg) :
    EventhubBase(cfg), _token_loaded(false), _subject("") {};

  bool authenticate(const std::string& jwtToken, const std::string& secret);
  bool isAuthenticated();
  bool allowPublish(const std::string& topic);
  bool allowSubscribe(const std::string& topic);
  bool allowCreateToken(const std::string& path);
  const std::string& subject();
  RateLimitConfig& getRateLimitConfig() { return _rlimit; };
};

} // namespace eventhub


