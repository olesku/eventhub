#pragma once

#include "Config.hpp"

namespace eventhub {

class EventhubBase {
public:
  explicit EventhubBase(Config& cfg) : _config(cfg) {};
  virtual ~EventhubBase() {};

  Config& config() {
    return _config;
  }

protected:
  Config& _config;

};

}