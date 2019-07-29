#include "access_controller.hpp"
#include <stdexcept>
#include <iostream>

/*
{
  "exp": 1300819380,
  "sub": "ole.skudsvik@gmail.com",
  "read": [ "channel1", "channel2" ],
  "write": [ "channel1", "channel2" ],
  "createtoken": [ ".*" ]
}
 */

namespace eventhub {

#define REQUIRE_TOKEN_LOADED(x) if (!_token_loaded) return false;

AccessController::AccessController() {
  _token_loaded = false;
}

AccessController::~AccessController() {

}

bool AccessController::loadToken(const std::string& jwt_token, const std::string& secret) {
  try {
    _token = jwt::decode(jwt_token, jwt::params::algorithms({"hs256"}), jwt::params::secret(secret));
  } catch(std::exception& e) {
    return false;
  }

  _token_loaded = true;

  return true;
}

bool AccessController::allowPublish(const std::string& topic) {
  REQUIRE_TOKEN_LOADED();

  //std::cout << _token.payload() << std::endl;

  auto &j = _token.payload().create_json_obj();
  

  int i = 0;
  for (auto it = j.begin(); it != j.end(); it++) {
    i++;
    std::cout << i << ": " << *it << std::endl;
  }

  return true;
}

bool AccessController::allowSubscribe(const std::string& topic) {
  REQUIRE_TOKEN_LOADED();

  return true;
}

bool AccessController::allowCreateToken(const std::string& path) {
  REQUIRE_TOKEN_LOADED();
  
  return true;
}

}
