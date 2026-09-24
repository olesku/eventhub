#include "http/Parser.hpp"

#include <string>
#include <utility>

#include "Util.hpp"
#include "http/picohttpparser.h"

namespace eventhub::http {

std::string Request::header(std::string name) const {
  Util::strToLower(name);
  const auto header = _headers.find(name);
  return header == _headers.end() ? std::string{} : header->second;
}

std::string Request::queryParameter(std::string name) const {
  Util::strToLower(name);
  const auto parameter = _query_parameters.find(name);
  return parameter == _query_parameters.end() ? std::string{} : parameter->second;
}

void Request::_reset() {
  _method.clear();
  _path.clear();
  _headers.clear();
  _query_parameters.clear();
}

void Request::_parseQueryString(const std::string& query) {
  std::size_t position = 0;
  while (position < query.size()) {
    const auto equals = query.find('=', position);
    if (equals == std::string::npos) {
      break;
    }

    const auto ampersand = query.find('&', equals + 1);
    auto name = query.substr(position, equals - position);
    auto value = query.substr(equals + 1,
                              ampersand == std::string::npos
                                  ? std::string::npos
                                  : ampersand - (equals + 1));
    position = ampersand == std::string::npos ? query.size() : ampersand + 1;

    if (!name.empty() && !value.empty()) {
      Util::strToLower(name);
      _query_parameters[name] = std::move(value);
    }
  }
}

Parser::Parser(ParserCallbacks callbacks) : _callbacks(std::move(callbacks)) {
  _resetState();
}

void Parser::parse(const char* data, std::size_t len) {
  if (_failed) {
    return;
  }

  if (_is_complete) {
    _resetState();
  }

  _bytes_read_prev = _bytes_read;
  if ((_bytes_read + len) > HTTP_BUFSIZ) {
    _failed = true;
    if (_callbacks.onError) {
      _callbacks.onError(ParseError::REQUEST_TOO_LARGE);
    }
    return;
  }

  _bytes_read += len;
  _buf.append(data, len);
  _phr_num_headers = sizeof(_phr_headers) / sizeof(_phr_headers[0]);

  const int result = phr_parse_request(
      _buf.c_str(), _bytes_read, &_phr_method, &_phr_method_len, &_phr_path,
      &_phr_path_len, &_phr_minor_version, _phr_headers, &_phr_num_headers,
      _bytes_read_prev);

  if (result == -1) {
    _failed = true;
    if (_callbacks.onError) {
      _callbacks.onError(ParseError::INVALID_REQUEST);
    }
    return;
  }

  // Incomplete input is internal parser state, not an application event.
  if (result == -2) {
    return;
  }

  if (_phr_method_len > 0) {
    _request._method.assign(_phr_method, _phr_method_len);
  }

  if (_phr_path_len > 0) {
    const std::string rawPath(_phr_path, _phr_path_len);
    const auto queryPosition = rawPath.find('?');
    if (queryPosition == std::string::npos) {
      _request._path = rawPath;
    } else {
      _request._path = rawPath.substr(0, queryPosition);
      _request._parseQueryString(rawPath.substr(queryPosition + 1));
    }
  }

  for (std::size_t i = 0; i < _phr_num_headers; ++i) {
    std::string name(_phr_headers[i].name, _phr_headers[i].name_len);
    std::string value(_phr_headers[i].value, _phr_headers[i].value_len);
    Util::strToLower(name);
    _request._headers[std::move(name)] = std::move(value);
  }

  _is_complete = true;
  if (_callbacks.onRequest) {
    _callbacks.onRequest(_request);
  }
}

void Parser::_resetState() {
  _buf.clear();
  _request._reset();
  _bytes_read        = 0;
  _bytes_read_prev   = 0;
  _is_complete       = false;
  _failed            = false;
  _phr_method        = nullptr;
  _phr_path          = nullptr;
  _phr_num_headers   = 0;
  _phr_method_len    = 0;
  _phr_path_len      = 0;
  _phr_minor_version = 0;
}

} // namespace eventhub::http
