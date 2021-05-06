#ifndef INCLUDE_ACCESSCONTROLLER_HPP_
#define INCLUDE_ACCESSCONTROLLER_HPP_

#include <memory>
#include <string>
#include <vector>

#include "Config.hpp"
#include "Forward.hpp"
#include "EventhubBase.hpp"
#include "jwt/jwt.hpp"

namespace eventhub {

class AccessController final : public EventhubBase {
private:
  bool _token_loaded;
  std::string _jwt_secret;
  jwt::jwt_object _token;
  std::vector<std::string> _publish_acl;
  std::vector<std::string> _subscribe_acl;

public:
  AccessController(Config &cfg) :
    EventhubBase(cfg), _token_loaded(false) {};

  bool authenticate(const std::string& jwtToken, const std::string& secret);
  bool isAuthenticated();
  bool allowPublish(const std::string& topic);
  bool allowSubscribe(const std::string& topic);
  bool allowCreateToken(const std::string& path);
};

} // namespace eventhub

#endif // INCLUDE_ACCESSCONTROLLER_HPP_
