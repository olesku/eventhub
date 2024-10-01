#pragma once

#include <stddef.h>
#include <stdint.h>
#include <chrono>
#include <string>

namespace eventhub {

class Util final {
public:
  static const std::string base64Encode(const unsigned char* buffer, std::size_t length);
  static const std::string uriDecode(const std::string& str);
  static std::string& strToLower(std::string& s);
  static int64_t getTimeSinceEpoch();
  static std::string getSSLErrorString(unsigned long e);
  static std::string getFileMD5Sum(std::string_view filePath);

private:
  Util() {}
  ~Util() {}
};

} // namespace eventhub


