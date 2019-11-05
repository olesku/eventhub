#ifndef EVENTHUB_CONFIG_HPP
#define EVENTHUB_CONFIG_HPP

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>

namespace eventhub {
namespace config {

class AlreadyAdded : public std::exception {
public:
  std::string msg;
  AlreadyAdded(const std::string& param) :
    msg("Parameter '"+ param + "' is already added") {};
  virtual const char* what() const throw() {
    return msg.c_str();;
  }
};

class InvalidValue : public std::exception {
public:
  std::string msg;
  InvalidValue(const std::string& param, const std::string& expectedType) :
    msg("Config parameter '" +  param + "' SSSSvalue has invalid type. Expected " + expectedType) {};
  virtual const char* what() const throw() {
    return msg.c_str();
  }
};

class InvalidParameter : public std::exception {
public:
  std::string msg;
  InvalidParameter(const std::string& param) :
    msg("Requested config parameter '" + param + "' does not exist") {};
  virtual const char* what() const throw() {
    return msg.c_str();
  }
};

class InvalidTypeRequested : public std::exception {
public:
  std::string msg;
  InvalidTypeRequested(const std::string& wrongType, const std::string& param) :
    msg("Invalid type '" + wrongType + "' requested for parameter '"+ param + "'") {};
  virtual const char* what() const throw() {
    return msg.c_str();
  }
};

enum class ValueType {
  STRING,
  INT,
  BOOL
};

class ConfigValue {
public:
  ValueType valueType;
  std::string strValue;
  int intValue;
  bool boolValue;
};

class EventhubConfig {
public:
  void addInt(const std::string& envName, int defaultValue);
  void addString(const std::string& envName, const std::string& defaultValue);
  void addBool(const std::string& envName, bool defaultValue);

  bool del(const std::string parameter);

  const std::string getString(const std::string& parameter);
  const int getInt(const std::string& parameter);
  const bool getBool(const std::string& parameter);

  static EventhubConfig& getInstance() {
    static EventhubConfig instance;
    return instance;
  }

  private:
    std::unordered_map<std::string, ConfigValue> _configMap;
    std::mutex _configMapLock;
};

} // namespace config
} // namespace eventhub

#define Config config::EventhubConfig::getInstance()
#endif
