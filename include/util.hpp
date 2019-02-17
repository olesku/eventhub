#ifndef EVENTHUB_UTIL_HPP
#define EVENTHUB_UTIL_HPP

#include <string>

namespace eventhub {
  class util {
    public:
      static const std::string base64_encode(const unsigned char* buffer, size_t length);

    private:
      util() {};
      ~util() {};
  };
}

#endif
