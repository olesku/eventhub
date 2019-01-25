#undef NDEBUG
#include <glog/logging.h>
#include <cctype>
#include <string>
#include <algorithm>

#define MAXEVENTS 1024

inline std::string& str_tolower(std::string& s) {
    std::transform(s.begin(), s.end(), s.begin(), 
                   [](unsigned char c){ return std::tolower(c); }
                  );
    return s;
}
