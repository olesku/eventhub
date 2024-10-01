#pragma once

#include <memory>
#include <string>
#include <vector>
#include <exception>

#include "Forward.hpp"
#include "EventhubBase.hpp"
#include "jwt/jwt.hpp"

namespace eventhub {

struct rlimit_config_t {
  std::string topic;
  unsigned long interval;
  unsigned long max;
};

typedef struct rlimit_config_t rlimit_config_t;

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
    const rlimit_config_t getRateLimitForTopic(std::string_view topic);
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

  bool authenticate(std::string_view jwtToken, std::string_view secret);
  bool isAuthenticated();
  bool allowPublish(std::string_view topic);
  bool allowSubscribe(std::string_view topic);
  bool allowCreateToken(std::string_view path);
  const std::string& subject();
  RateLimitConfig& getRateLimitConfig() { return _rlimit; };
};

} // namespace eventhub


