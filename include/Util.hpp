#ifndef EVENTHUB_UTIL_HPP
#define EVENTHUB_UTIL_HPP

#include <string>

namespace eventhub {
class Util {
public:
  static const std::string base64Encode(const unsigned char* buffer, size_t length);

private:
  Util(){};
  ~Util(){};
};
} // namespace eventhub

#endif
