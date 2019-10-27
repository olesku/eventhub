#include <chrono>
#include <iostream>
#include <sstream>
#include <string>

extern "C" {
#include "hashids.h"
}

/*
$MESSAGE
message-id: blah
message-length: blah
topic: "blah"

DATA
 */

/*
{
  id: "blah",
  topic: "blah"
  data: base64(message)
}
*/

namespace eventhub {
class Message {
private:
  std::string _id;
  std::string _topic;
  std::string _data;

  /*
      Generates a hash using unixtime in milliseconds, length of message and memory address
      of this class as input to the hashids algorithm. This should be unique in most cases.
    */
  std::string _generateId() {
    std::chrono::nanoseconds unixtime_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch());

    unsigned long long n[] = {static_cast<unsigned long long>(unixtime_ns.count()), _data.length(), reinterpret_cast<unsigned long long>(this)};
    char buf[64];

    // TODO: Don't initalize this for each message.
    hashids_t* hid = ::hashids_init("eventhub_salt");
    hashids_encode(hid, buf, 3, n);
    hashids_free(hid);

    return buf;
  }

public:
  Message(const std::string& topic, const std::string& data) {
    _topic = topic;
    _data  = data;
    _id    = _generateId();
  };

  ~Message(){};

  std::string to_string() {
    std::stringstream ss;

    ss << "$message\r\n";
    ss << "$id: " << _id << "\r\n";
    ss << "$length: " << _data.length() << "\r\n";
    ss << "$topic: " << _topic << "\r\n";
    ss << "\r\n";
    ss << _data << "\r\n";

    return ss.str();
  }
};
} // namespace eventhub
