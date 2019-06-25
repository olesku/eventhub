#ifndef EVENTHUB_HTTP_RESPONSE_H
#define EVENTHUB_HTTP_RESPONSE_H

#include <string>
#include <unordered_map>

typedef std::unordered_map<std::string, std::string> HeaderList_t;

class HTTPResponse {
public:
  HTTPResponse(int statusCode = 200, const std::string body = "");
  void SetStatus(int status, std::string statusMsg);
  void SetStatus(int status);
  void SetHeader(const std::string& name, const std::string& value);
  void SetBody(const std::string& data);
  void AppendBody(const std::string& data);
  const std::string GetStatusMsg(int statusCode);
  const std::string Get();

private:
  int m_statusCode;
  std::string m_statusMsg;
  std::string m_body;
  HeaderList_t m_headers;
};

#endif
