#include <string>
#include <sstream>
#include <chrono>
#include <iostream>

extern "C" {
  #include "hashids.h"
}

/*
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
*/

/*
{
  "id": id
  "topic": topic
  "data": ""
}
*/

/*
$MESSAGE
message-id: blah
message-length: blah
topic: "blah"

DATA
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
        std::chrono::system_clock::now().time_since_epoch()
      );

      unsigned long long n[] = { static_cast<unsigned long long>(unixtime_ns.count()), _data.length(), reinterpret_cast<unsigned long long>(this)};
      char buf[64];

      hashids_t *hid = ::hashids_init("eventhub_salt");
      hashids_encode(hid, buf, 3, n);
      hashids_free(hid);

      return buf;
    }


  public:
    Message(const std::string& topic, const std::string& data) {
      _topic = topic;
      _data = data;
      _id = _generateId();
    };

    ~Message() {};

    std::string get() {
      std::stringstream ss;

      ss << "$message\r\n";
      ss << "$id: " << _id << "\r\n";
      ss << "$length: " << _data.length() << "\r\n";
      ss << "$topic: " << _topic << "\r\n";
      ss << "\r\n";
      ss << _data << "\r\n";

      return ss.str();
    }

  /*
    std::string get() {
      rapidjson::Document doc;
      rapidjson::Value header(rapidjson::kObjectType);
      rapidjson::StringBuffer sb;
      rapidjson::Writer<rapidjson::StringBuffer> writer(sb);



      doc.SetObject();

      header.AddMember("message-id", "", doc.GetAllocator());
      header["message-id"].SetString(_generateId().c_str(), doc.GetAllocator());

      header.AddMember("content-length", 0, doc.GetAllocator());
      header["content-length"].SetInt(_data.length());

      header.AddMember("topic", "", doc.GetAllocator());
      header["topic"].SetString(_topic.c_str(), doc.GetAllocator());

      //header.AddMember("length", _data.l<ength(), doc.GetAllocator());

     doc.AddMember("_header", header, doc.GetAllocator());

     doc.AddMember("data", "", doc.GetAllocator());
     doc["data"].SetString(_data.c_str(), doc.GetAllocator());

    doc.Accept(writer);

    return sb.GetString();

      return "";
    }
    */
};
}
