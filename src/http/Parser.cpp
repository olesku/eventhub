#include "http/Parser.hpp"

#include <string.h>

#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

#include "Common.hpp"
#include "Util.hpp"
#include "http/picohttpparser.h"

namespace eventhub {
namespace http {

/**
  Constructor.
**/
Parser::Parser() {
  _bytes_read      = 0;
  _bytes_read_prev = 0;
  _is_complete     = false;
  _error_message   = "";
  _callback        = [](http::Parser* req, http::RequestState reqState) {
        LOG->error("Websocket parser callback was called before it was initialized.");
  };
}

/**
  Destructor.
**/
Parser::~Parser() {}

/**
    Parse the request.
    @param data Raw http request data.
    @param len Length of data.
  **/
void Parser::parse(const char* data, int len) {
  int pret;

  if (_is_complete)
    return _callback(this, RequestState::REQ_OK);
  _bytes_read_prev = _bytes_read;

  // Request is to large.
  if ((_bytes_read + len) > HTTP_BUFSIZ) {
    _error_message = "REQ_TO_BIG: Request to large.";
    return _callback(this, RequestState::REQ_TO_BIG);
  }

  _bytes_read += len;
  _buf.append(data, len);

  _phr_num_headers = sizeof(_phr_headers) / sizeof(_phr_headers[0]);

  pret = phr_parse_request(_buf.c_str(), _bytes_read, &_phr_method, &_phr_method_len, &_phr_path,
                           &_phr_path_len, &_phr_minor_version, _phr_headers, &_phr_num_headers, _bytes_read_prev);

  // Parse error.
  if (pret == -1) {
    _error_message = "REQ_FAILED: Parse failed.";
    return _callback(this, RequestState::REQ_FAILED);
  }

  // Request incomplete.
  if (pret == -2) {
    return _callback(this, RequestState::REQ_INCOMPLETE);
  }

  if (_phr_method_len > 0)
    _method.insert(0, _phr_method, _phr_method_len);

  if (_phr_path_len > 0) {
    std::string rawPath;
    rawPath.insert(0, _phr_path, _phr_path_len);

    size_t qsPos = rawPath.find_first_of('?', 0);
    if (qsPos != std::string::npos) {
      std::string qStr;
      qStr  = rawPath.substr(qsPos + 1, std::string::npos);
      _path = rawPath.substr(0, qsPos);
      _parse_query_string(qStr);
    } else {
      _path = rawPath.substr(0, rawPath.find_last_of(' ', 0));
    }
  }

  for (int i = 0; i < static_cast<int>(_phr_num_headers); i++) {
    std::string name, value;
    name.insert(0, _phr_headers[i].name, _phr_headers[i].name_len);
    value.insert(0, _phr_headers[i].value, _phr_headers[i].value_len);
    Util::strToLower(name);
    _headers[name] = value;
  }

  _is_complete      = true;
  _buf[_bytes_read] = '\0';

  return _callback(this, RequestState::REQ_OK);
}

/**
    Get the HTTP request path.
  **/
const std::string& Parser::getPath() {
  return _path;
}

/**
    Get the HTTP request method.
  **/
const std::string& Parser::getMethod() {
  return _method;
}

/**
    Get a spesific header.
    @param header Header to get.
  **/
const std::string Parser::getHeader(std::string header) {
  Util::strToLower(header);

  if (_headers.find(header) != _headers.end()) {
    return _headers[header];
  }

  return "";
}

const std::map<std::string, std::string>& Parser::getHeaders() {
  return _headers;
}

/**
    Extracts query parameters from a string if they exist.
    @param buf The string to parse.
  **/
size_t Parser::_parse_query_string(const std::string& buf) {
  size_t prevpos = 0, eqlpos = 0;

  while ((eqlpos = buf.find("=", prevpos)) != std::string::npos) {
    std::string param, val;
    size_t len;

    len = buf.find("&", eqlpos);

    if (len != std::string::npos)
      len = (len - eqlpos);
    else
      len = (buf.size() - eqlpos);

    param = buf.substr(prevpos, (eqlpos - prevpos));

    eqlpos++;
    val     = buf.substr(eqlpos, len - 1);
    prevpos = eqlpos + len;

    if (!param.empty() && !val.empty()) {
      Util::strToLower(param);
      _qsmap[param] = val;
    }
  }

  return _qsmap.size();
}

/**
    Get a spesific query string parameter.
    @param param Parameter to get.
  **/
const std::string Parser::getQueryString(std::string param) {
  Util::strToLower(param);

  if (_qsmap.find(param) != _qsmap.end()) {
    return _qsmap[param];
  }

  return "";
}

/**
    Returns number of query strings in the request.
  **/
size_t Parser::numQueryString() {
  return _qsmap.size();
}

const std::string& Parser::getErrorMessage() {
  return _error_message;
}

void Parser::setCallback(ParserCallback callback) {
  _callback = callback;
}

} // namespace http
} // namespace eventhub
