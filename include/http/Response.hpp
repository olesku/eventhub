#pragma once

#include <string>
#include <unordered_map>

namespace eventhub {
namespace http {

typedef std::unordered_map<std::string, std::string> HeaderList_t;

class Response final {
public:
  explicit Response(int statusCode = 200, const std::string& body = "");
  void setStatus(int status, const std::string& statusMsg);
  void setStatus(int status);
  void setHeader(const std::string& name, const std::string& value);
  void setBody(const std::string& data);
  void appendBody(const std::string& data);
  const std::string getStatusMsg(int statusCode);
  const std::string get();

private:
  int _statusCode;
  std::string _statusMsg;
  std::string _body;
  HeaderList_t _headers;
};

} // namespace http
} // namespace eventhub

