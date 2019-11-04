#include "Config.hpp"
#include <string>
#include <string.h>
#include <cstdlib>
#include <unordered_map>
#include <memory>
#include <utility>
#include <mutex>

using namespace std;

namespace eventhub {
namespace config {

template<>
void EventhubConfig::add<int>(std::string name, int defaultValue) {
  std::lock_guard<std::mutex> lock(_configMapLock);
  ConfigValue val;
  val.valueType = ValueType::INT;

  if (_configMap.find(name) != _configMap.end()) {
    throw AlreadyAdded(name);
  }

  auto envVal = getenv(name.c_str());
  if (envVal == NULL) {
    val.intValue = defaultValue;
  } else {
    try {
      val.intValue = std::stoi(envVal);
    } catch(...) {
      throw InvalidValue(name, "integer");
    }
  }

  _configMap.emplace(std::make_pair(name, val));
}

template<>
void EventhubConfig::add<std::string>(std::string name, std::string defaultValue) {
  std::lock_guard<std::mutex> lock(_configMapLock);
  ConfigValue val;
  val.valueType = ValueType::STRING;

  if (_configMap.find(name) != _configMap.end()) {
    throw AlreadyAdded(name);
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
void EventhubConfig::add<bool>(std::string name, bool defaultValue) {
  std::lock_guard<std::mutex> lock(_configMapLock);
  ConfigValue val;
  val.valueType = ValueType::BOOL;
  val.boolValue = false;

  if (_configMap.find(name) != _configMap.end()) {
    throw AlreadyAdded(name);
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
    } else if (memcmp(envVal, "false", envLen) == 0 ||
        memcmp(envVal, "FALSE", envLen) == 0 ||
        memcmp(envVal, "0", envLen) == 0)
    {
      val.boolValue = false;
    } else {
      throw InvalidValue(name, "boolean");
    }
  }

  _configMap.emplace(std::make_pair(name, val));
}

template<>
const std::string EventhubConfig::get<std::string>(const std::string parameter) {
  std::lock_guard<std::mutex> lock(_configMapLock);
  if (_configMap.find(parameter) == _configMap.end()) {
    throw InvalidParameter(parameter);
  }

  if (_configMap[parameter].valueType != ValueType::STRING) {
    throw InvalidTypeRequested("<string>", parameter);
  }

  return _configMap[parameter].strValue;
}

bool EventhubConfig::del(const std::string parameter) {
  std::lock_guard<std::mutex> lock(_configMapLock);
  auto it = _configMap.find(parameter);
  if (it != _configMap.end()) {
    _configMap.erase(it);
    return true;
  }

  return false;
}

template<>
const int EventhubConfig::get<int>(const std::string parameter) {
  std::lock_guard<std::mutex> lock(_configMapLock);
  if (_configMap.find(parameter) == _configMap.end()) {
    throw InvalidParameter(parameter);
  }

  if (_configMap[parameter].valueType != ValueType::INT) {
    throw InvalidTypeRequested("<int>", parameter);
  }

  return _configMap[parameter].intValue;
}

template<>
const bool EventhubConfig::get<bool>(const std::string parameter) {
  std::lock_guard<std::mutex> lock(_configMapLock);
  if (_configMap.find(parameter) == _configMap.end()) {
    throw InvalidParameter(parameter);
  }

  if (_configMap[parameter].valueType != ValueType::BOOL) {
    throw InvalidTypeRequested("<bool>", parameter);
  }

  return _configMap[parameter].boolValue;
}

} // namespace config
} // namespace eventhub