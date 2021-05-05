#ifndef INCLUDE_EVENTHUBBASE_HPP_
#define INCLUDE_EVENTHUBBASE_HPP_

#include "Config.hpp"

namespace eventhub {
using namespace evconfig;

class EventhubBase {
public:
  explicit EventhubBase(Config& cfg) : _config(cfg) {};
  ~EventhubBase() {};

  Config& config() {
    return _config;
  }

protected:
  Config& _config;

};

}

#endif