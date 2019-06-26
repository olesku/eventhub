#include <string>
#include <sstream>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

/*
{
  "id": id
  "topic": topic
  "data": ""
}
*/

namespace eventhub {
class Message {
  private:
    std::string _id;
    std::string _topic;
    std::string _data;
    std::string _message;

    std::string _generateId() {
      return "testid";
    }

  public:
    Message(const std::string& topic, const std::string& data) {
      _topic = topic;
      _data = data;
    };

    ~Message() {};

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
};
}
