#include <stdlib.h>
#include <ctype.h>
#include <memory>
#include <sstream>
#include <fstream>
#include <string>
#include <unordered_map>

#include "Config.hpp"

namespace eventhub {
Config::Config() {}

void Config::_loadFromStream(std::istream& data, const std::string& path) {
  std::fstream f;
  std::string line;
  unsigned int lineNo = 0;

  while (std::getline(data, line)) {
    std::string parsedKey, parsedValue;
    std::size_t pos = 0;

    lineNo++;

    // Ignore all leading whitespaces.
    for (char c = line[pos]; pos < line.length() && (c == ' ' || c == '\t'); c = line[++pos]);

    // Ignore empty lines and comments.
    if (pos == line.length() || line[pos] == '#')
      continue;

    // Parse key name until space, tab or equal sign is reached.
    for (char c = line[pos]; pos < line.length(); c = line[++pos]) {
      if (c == ' ' || c == '\t' || c == '=')
        break;

      parsedKey += c;
    }

    // Key should not be empty.
    if (parsedKey.empty())
      throw SyntaxErrorException{path, lineNo};

    // We should now be at the equal sign before the value.
    for (char c = line[pos]; pos < line.length() && c != '='; c = line[++pos]);

    if (line[pos] != '=')
      throw SyntaxErrorException{path, lineNo};

    // Parse value ignoring leading whitespace and quotes.
    for (char c = line[++pos]; pos < line.length() && (c == ' ' || c == '\t' || c == '"' || c == '\''); c = line[++pos]);
    while (pos < line.length()) {
      if (pos+1 == line.length() && (line[pos] == '"' || line[pos] == '\''))
        break;
      if (line[pos] != '\r')
        parsedValue += line[pos];
      pos++;
    }

    if (parsedValue.empty())
      throw SyntaxErrorException{path, lineNo};

    // Update config object with parsed key and value.
    auto opt = _options.find(parsedKey);
    if (opt == _options.end())
      throw InvalidConfigOptionException{parsedKey, path, lineNo};

    opt->second->_set(parsedValue);
  }
}

void Config::_loadFromFile(const std::string& path) {
  std::fstream f;
  f.open(path, std::fstream::in);
  _loadFromStream(f, path);
  f.close();
}

void strToUpper(std::string& str) {
  for (auto it = str.begin(); it != str.end(); it++) {
    *it = toupper(*it);
  }
}

void Config::_loadFromEnv() {
  for (const auto& it_opt : _options) {
    auto optName = it_opt.first;
    auto val = getenv(optName.c_str());
    if (val != nullptr) {
      it_opt.second->_set(val);
      it_opt.second->_hasValue = true;
    } else {
      strToUpper(optName);
      val = getenv(optName.c_str());
      if (val != nullptr) {
        it_opt.second->_set(val);
        it_opt.second->_hasValue = true;
      }
    }
  }
}

void Config::load() {
  if (!_cfgStreamBuf.str().empty())
    _loadFromStream(_cfgStreamBuf, "");

  if (!_cfgFile.empty())
    _loadFromFile(_cfgFile);

  if (_loadEnv)
    _loadFromEnv();

  for (const auto& it_opt : _options) {
    auto const& opt = *it_opt.second;
    if (opt._settings == ConfigValueSettings::REQUIRED && !opt._hasValue)
      throw RequiredOptionMissingException(opt.name());
  }
}

void Config::clearValues() {
  for (auto& opt : _options) {
    opt.second->clear();
  }
}
}
