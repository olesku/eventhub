#ifndef EVENTHUB_ACCESSCONTROLLER_HPP
#define EVENTHUB_ACCESSCONTROLLER_HPP

#include <string>
#include <memory>
#include <vector>
#include "jwt/jwt.hpp"
#include "connection.hpp"

namespace eventhub {
  class AccessController {
    private:
      bool _token_loaded;
      jwt::jwt_object _token;
      std::vector<std::string> _publish_acl;
      std::vector<std::string> _subscribe_acl;

    public:
      AccessController();
      ~AccessController();

      bool loadToken(const std::string& jwt_token, const std::string& secret);
      bool allowPublish(const std::string& topic);
      bool allowSubscribe(const std::string& topic);
      bool allowCreateToken(const std::string& path);
  };
}

#endif
