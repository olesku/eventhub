#ifndef EVENTHUB_ACCESSCONTROLLER_HPP
#define EVENTHUB_ACCESSCONTROLLER_HPP

#include "jwt/jwt.hpp"
#include <memory>
#include <string>
#include <vector>

namespace eventhub {
class AccessController {
private:
  bool _token_loaded;
  std::string _jwt_secret;
  jwt::jwt_object _token;
  std::vector<std::string> _publish_acl;
  std::vector<std::string> _subscribe_acl;

public:
  AccessController();
  ~AccessController();

  bool authenticate(const std::string& jwt_token, const std::string& secret);
  bool isAuthenticated();
  bool allowPublish(const std::string& topic);
  bool allowSubscribe(const std::string& topic);
  bool allowCreateToken(const std::string& path);
};
} // namespace eventhub

#endif
