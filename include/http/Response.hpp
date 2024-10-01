#pragma once

#include <string>
#include <unordered_map>

namespace eventhub {
namespace http {

typedef std::unordered_map<std::string, std::string> HeaderList_t;

class Response final {
public:
  explicit Response(int statusCode = 200, std::string_view body = "");
  void setStatus(int status, std::string_view statusMsg);
  void setStatus(int status);
  void setHeader(std::string_view name, std::string_view value);
  void setBody(std::string_view data);
  void appendBody(std::string_view data);
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

