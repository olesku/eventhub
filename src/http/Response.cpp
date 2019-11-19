#include "http/Response.hpp"

#include <sstream>
#include <string>

#include "Common.hpp"

namespace eventhub {
namespace http {

#define CRLF "\r\n"

Response::Response(int statusCode, const std::string body) {
  _statusCode = statusCode;
  _statusMsg  = getStatusMsg(statusCode);
  _body       = body;
}

void Response::setStatus(int status, std::string statusMsg) {
  _statusCode = status;
  _statusMsg  = statusMsg;
}

void Response::setStatus(int status) {
  _statusCode = status;
  _statusMsg  = getStatusMsg(status);
  ;
}

void Response::setHeader(const std::string& name, const std::string& value) {
  _headers[name] = value;
}

void Response::setBody(const std::string& data) {
  _body = data;
}

void Response::appendBody(const std::string& data) {
  _body.append(data);
}

const std::string Response::get() {
  std::stringstream ss;

  try {
    _headers.at("Connection");
    _headers.at("Content-Type");

    if ((_headers["Content-Type"].compare("text/event-stream") != 0) && (_headers["Connection"].compare("close") == 0) && _body.size() > 0) {
      setHeader("Content-Length", std::to_string(_body.size()));
    }
  } catch (...) {}

  try {
    _headers.at("Content-Type");
  } catch (...) {
    if (_statusCode == 200 && !_body.empty())
      setHeader("Content-Type", "text/html");
  }

  ss << "HTTP/1.1 " << _statusCode << " " << _statusMsg << CRLF;

  for (const auto& header : _headers) {
    ss << header.first << ": " << header.second << CRLF;
  }

  ss << CRLF << _body;

  return ss.str();
}

/**
 Translate a HTTP statuscode into a explanatory string.
 @param statusCode statuscode to translate.
**/
const std::string Response::getStatusMsg(int statusCode) {
  switch (statusCode) {
    case 200:
      return "OK";
    case 204:
      return "No Content";
    case 100:
      return "Continue";
    case 101:
      return "Switching Protocols";
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 405:
      return "Method not allowed";
    case 411:
      return "Length Required";
    case 413:
      return "Request Entity Too Large";
  }

  return "OK";
}

} // namespace http
} // namespace eventhub
