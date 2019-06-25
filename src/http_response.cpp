#include "http_response.hpp"
#include "common.hpp"
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <sstream>

#define CRLF "\r\n"

HTTPResponse::HTTPResponse(int statusCode, const std::string body) {
  m_statusCode = statusCode;
  m_statusMsg  = GetStatusMsg(statusCode);
  m_body       = body;
}

void HTTPResponse::SetStatus(int status, std::string statusMsg) {
  m_statusCode = status;
  m_statusMsg  = statusMsg;
}

void HTTPResponse::SetStatus(int status) {
  m_statusCode = status;
  m_statusMsg  = GetStatusMsg(status);
  ;
}

void HTTPResponse::SetHeader(const std::string& name, const std::string& value) {
  m_headers[name] = value;
}

void HTTPResponse::SetBody(const std::string& data) {
  m_body = data;
}

void HTTPResponse::AppendBody(const std::string& data) {
  m_body.append(data);
}

const std::string HTTPResponse::Get() {
  std::stringstream ss;

  try {
    m_headers.at("Connection");
    m_headers.at("Content-Type");

    if ((m_headers["Content-Type"].compare("text/event-stream") != 0) && (m_headers["Connection"].compare("close") == 0) && m_body.size() > 0) {
      SetHeader("Content-Length", boost::lexical_cast<std::string>(m_body.size()));
    }
  } catch (...) {}

  try {
    m_headers.at("Content-Type");
  } catch (...) {
    if (m_statusCode == 200 && !m_body.empty())
      SetHeader("Content-Type", "text/html");
  }

  ss << "HTTP/1.1 " << m_statusCode << " " << m_statusMsg << CRLF;

  BOOST_FOREACH (HeaderList_t::value_type& header, m_headers) {
    ss << header.first << ": " << header.second << CRLF;
  }

  ss << CRLF << m_body;

  return ss.str();
}

/**
 Translate a HTTP statuscode into a explanatory string.
 @param statusCode statuscode to translate.
**/
const std::string HTTPResponse::GetStatusMsg(int statusCode) {
  switch (statusCode) {
    case 200:
      return "OK";
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
    case 411:
      return "Length Required";
    case 413:
      return "Request Entity Too Large";
  }

  return "OK";
}
