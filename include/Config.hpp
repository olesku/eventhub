#ifndef EVENTHUB_CONFIG_HPP
#define EVENTHUB_CONFIG_HPP

#include <string>
#include <unordered_map>
#include <memory>

namespace eventhub {

//JWT_SECRET, "", true
/*
{ "LISTEN_PORT", "8080" }
{ "WORKER_THREADS", 0 }
{ "JWT_SECRET", "" }
{ "REDIS_HOST", "127.0.0.1" }
{ "REDIS_PORT", "6379" }
{ "REDIS_PASSWORD", "" }
{ "REDIS_PREFIX", "eventhub" }
{ "MAX_CACHE_LENGTH", "1000" }
{ "PING_INTERVAL", "10" }

get<int>("LISTEN_PORT");
get<std::string>("JWT_SECRET");
*/

class AlreadyExist : public std::exception {
public:
  virtual const char* what() const throw() {
    return "Config option already exist";
  }
};

class InvalidValue : public std::exception {
public:
  virtual const char* what() const throw() {
    return "Config parameter value has invalid format";
  }
};

class InvalidParameter : public std::exception {
public:
  virtual const char* what() const throw() {
    return "Requested config parameter does not exist";
  }
};

class InvalidType : public std::exception {
public:
  virtual const char* what() const throw() {
    return "Invalid type requested for config parameter";
  }
};

enum class ValueType {
  STRING,
  INT,
  BOOL
};

class EventhubConfig {
public:
  class ConfigValue {
  public:
    ValueType valueType;
    std::string strValue;
    int intValue;
    bool boolValue;
  };


  template <typename T>
  void addOption(std::string envName, T defaultValue);

  template <typename T>
  const T get(const std::string parameter);

  // int get(const std::string parameter);



  static EventhubConfig& getInstance() {
    static EventhubConfig instance;
    return instance;
  }

  const std::string getJWTSecret() {
    return "eventhub_secret";
  }

  unsigned int getPingInterval() {
    return 10;
  }

  const std::string getRedisPrefix() {
    return "eventhub";
  }


  /* Max number of items to cache for a given topic.
   * Set to 0 for unlimited cache length.
   */
  long long getMaxCacheLength() {
    return 1000;
  }

  private:
    std::unordered_map<std::string, ConfigValue> _configMap;
};

} // namespace eventhub

#define Config EventhubConfig::getInstance()

#endif
