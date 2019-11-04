#include "Config.hpp"
#include <string>
#include <string.h>
#include <cstdlib>
#include <unordered_map>
#include <memory>
#include <utility>

using namespace std;

namespace eventhub {

template<>
void EventhubConfig::addOption<int>(std::string name, int defaultValue) {
  ConfigValue val;
  val.valueType = ValueType::INT;

  if (_configMap.find(name) != _configMap.end()) {
    throw AlreadyExist();
  }

  auto envVal = getenv(name.c_str());
  if (envVal == NULL) {
    val.intValue = defaultValue;
  } else {
    try {
      val.intValue = std::stoi(envVal);
    } catch(...) {
      throw InvalidValue();
    }
  }

  _configMap.emplace(std::make_pair(name, val));
}

template<>
void EventhubConfig::addOption<std::string>(std::string name, std::string defaultValue) {
  ConfigValue val;
  val.valueType = ValueType::STRING;

  if (_configMap.find(name) != _configMap.end()) {
    throw AlreadyExist();
  }

  auto envVal = getenv(name.c_str());
  if (envVal == NULL) {
    val.strValue = defaultValue;
  } else {
    val.strValue = envVal;
  }

  _configMap.emplace(std::make_pair(name, val));
}

template<>
void EventhubConfig::addOption<bool>(std::string name, bool defaultValue) {
  ConfigValue val;
  val.valueType = ValueType::BOOL;
  val.boolValue = false;

  if (_configMap.find(name) != _configMap.end()) {
    throw AlreadyExist();
  }

  auto envVal = getenv(name.c_str());
  if (envVal == NULL) {
    val.boolValue = defaultValue;
  } else {
    size_t envLen = strlen(envVal);
    if (memcmp(envVal, "true", envLen) == 0 ||
        memcmp(envVal, "TRUE", envLen) == 0 ||
        memcmp(envVal, "1", envLen) == 0) 
    {
      val.boolValue = true;
    } else {
      throw InvalidValue();
    }
  }

  _configMap.emplace(std::make_pair(name, val));
}

template<>
const std::string EventhubConfig::get<std::string>(const std::string parameter) {
  if (_configMap.find(parameter) == _configMap.end()) {
    throw InvalidParameter();
  }

  if (_configMap[parameter].valueType != ValueType::STRING) {
    throw InvalidType();
  }

  return _configMap[parameter].strValue;
}

template<>
const int EventhubConfig::get<int>(const std::string parameter) {
  if (_configMap.find(parameter) == _configMap.end()) {
    throw InvalidParameter();
  }

  if (_configMap[parameter].valueType != ValueType::INT) {
    throw InvalidType();
  }

  return _configMap[parameter].intValue;
}

template<>
const bool EventhubConfig::get<bool>(const std::string parameter) {
  if (_configMap.find(parameter) == _configMap.end()) {
    throw InvalidParameter();
  }

  if (_configMap[parameter].valueType != ValueType::BOOL) {
    throw InvalidType();
  }

  return _configMap[parameter].boolValue;
}

}
