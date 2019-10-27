#ifndef EVENTHUB_COMMON_HPP
#define EVENTHUB_COMMON_HPP

#undef NDEBUG
#include <algorithm>
#include <cctype>
#include <glog/logging.h>
#include <string>

#define EPOLL_MAX_TIMEOUT 100
#define MAXEVENTS 1024

inline std::string& strTolower(std::string& s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

#endif
