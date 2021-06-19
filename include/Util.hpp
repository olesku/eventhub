#ifndef INCLUDE_UTIL_HPP_
#define INCLUDE_UTIL_HPP_

#include <chrono>
#include <string>

namespace eventhub {

class Util {
public:
  static const std::string base64Encode(const unsigned char* buffer, size_t length);
  static const std::string uriDecode(const std::string& str);
  static std::string& strToLower(std::string& s);
  static int64_t getTimeSinceEpoch();
  static std::string getSSLErrorString(unsigned long e);
  static std::string getFileMD5Sum(const std::string& filePath);

private:
  Util() {}
  ~Util() {}
};

} // namespace eventhub

#endif // INCLUDE_UTIL_HPP_
