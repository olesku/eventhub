#ifndef __INCLUDE_CONFIG_HPP__
#define __INCLUDE_CONFIG_HPP__

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace evconfig {

enum class ValueSettings : uint8_t {
  REQUIRED,
  OPTIONAL
};

enum class ValueType : uint8_t {
  INT,
  BOOL,
  STRING
};

struct OptionDefinition {
  std::string name;
  ValueType type;
  std::string defaultValue;
  ValueSettings settings;
};

using ConfigValue = std::variant<int, bool, std::string>;
using ConfigMap = std::vector<OptionDefinition>;

template <class... Ts>
struct overload : Ts... { using Ts::operator()...; };
template <class... Ts>
overload(Ts...) -> overload<Ts...>;

class InvalidConfigOptionException : public std::exception {
public:
  std::string msg;

  InvalidConfigOptionException(std::string optName, std::string path = "", unsigned int lineNo = 0) {
    if (!path.empty() && lineNo > 0)
      msg = "Invalid config option \"" + optName + "\" at " + path + ":" + std::to_string(lineNo);
    else
      msg = "Invalid config option \"" + optName;
  }

  const char* what() const throw() {
    return msg.c_str();
  }
};

class SyntaxErrorException : public std::runtime_error {
public:
  SyntaxErrorException(std::string path, unsigned int lineNo) : runtime_error("Invalid syntax at " + path + ":" + std::to_string(lineNo)) {}
};

class RequiredOptionMissingException : public std::runtime_error {
public:
  RequiredOptionMissingException(const std::string optName) : runtime_error("Missing required option \"" + optName + "\"") {}
};

class ConfigOption {
  friend class Config;

public:
  ConfigOption(const std::string& optName, ValueSettings settings) : _name(optName), _settings(settings), _hasValue(false){};

  ~ConfigOption(){};

protected:
  const std::string& name() const {
    return _name;
  }

  void _set(const std::string& value) {
    std::visit(overload{
                   [&](int) {
                     try {
                       _value    = std::stoi(value);
                       _hasValue = true;
                     } catch (...) {
                       throw std::runtime_error{"Invalid number: " + value};
                     }
                   },

                   [&](bool) {
                     if (value == "true" || value == "TRUE" || value == "1") {
                       _value    = true;
                       _hasValue = true;
                     }

                     else if (value == "false" || value == "FALSE" || value == "0") {
                       _value    = false;
                       _hasValue = true;
                     }

                     else
                       throw std::runtime_error{"Invalid boolean value: " + value};
                   },

                   [&](std::string) {
                     _value    = value;
                     _hasValue = true;
                   }},
               _value);
  }

  template <typename T>
  const T& _get() const {
    const auto* v = std::get_if<T>(&_value);
    if (v == nullptr)
      throw std::runtime_error{"Requested invalid type for config parameter \"" + _name + "\""};
    return *v;
  }

  void clear() {
    _value    = {};
    _hasValue = false;
  }

private:
  std::string _name;
  ValueSettings _settings;
  ConfigValue _value;
  bool _hasValue;
};

class Config {
public:
  Config();

  Config(const ConfigMap& cfgMap) {
        for (const auto& cfgElm : cfgMap) {
      switch(cfgElm.type) {
        case ValueType::INT:
          defineOption<int>(cfgElm.name, cfgElm.settings);
        break;

        case ValueType::BOOL:
          defineOption<bool>(cfgElm.name, cfgElm.settings);
        break;

        case ValueType::STRING:
          defineOption<std::string>(cfgElm.name, cfgElm.settings);
        break;
      }

      if (!cfgElm.defaultValue.empty())
        _cfgStreamBuf << cfgElm.name + " = " << cfgElm.defaultValue << "\n";
    }
  };

  template <typename T>
  void defineOption(const std::string& optName, ValueSettings settings = ValueSettings::REQUIRED) {
    if (_options.find(optName) != _options.end())
      throw(std::runtime_error{"Redefinition of option \"" + optName + " \""});

    _options[optName]         = std::make_unique<ConfigOption>(optName, settings);
    _options[optName]->_value = T{};
  }

  template <typename T>
  const T& get(const std::string& optName) const {
    auto opt = _options.find(optName);
    if (opt == _options.end())
      throw InvalidConfigOptionException{optName};

    return opt->second->_get<T>();
  }

  void setFile(const std::string& fileName) { _cfgFile = fileName; };
  void setLoadFromEnv(bool shouldLoad) { _loadEnv = true; }
  void clearValues();
  void load();

  Config& operator<<(const char* str) {
    _cfgStreamBuf << str;
    return *this;
  };

private:
  std::unordered_map<std::string, std::unique_ptr<ConfigOption>> _options;
  std::stringstream _cfgStreamBuf;
  std::string _cfgFile;

  bool _loadEnv;
  void _loadFromStream(std::istream& data, const std::string& path = "");
  void _loadFromFile(const std::string& path);
  void _loadFromEnv();
};
} // namespace evconfig

#endif