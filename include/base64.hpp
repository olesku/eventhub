#ifndef __EVENTHUB_BASE64_H__
#define __EVENTHUB_REACTOR_BASE64_H__

#include <string>

namespace base64 {
	// * http://stackoverflow.com/a/5291537
	extern std::string encode(const std::string& bindata);
}

#endif
