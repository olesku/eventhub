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
    auto name            = query.substr(position, equals - position);
    auto value           = query.substr(equals + 1,
                              ampersand == std::string::npos
                                            ? std::string::npos
                                            : ampersand - (equals + 1));
    position             = ampersand == std::string::npos ? query.size() : ampersand + 1;

    if (!name.empty() && !value.empty()) {
      Util::strToLower(name);
      _query_parameters[name] = std::move(value);
    }
  }
}

Parser::Parser(ParserCallbacks callbacks) : _callbacks(std::move(callbacks)) {
  _resetState();
}

ParseResult Parser::parse(const char* data, std::size_t len) {
  if (_failed) {
    return {0, ParseStatus::FAILED};
  }

  if (_is_complete) {
    _resetState();
  }

  const std::size_t previousSize = _buf.size();
  _bytes_read_prev               = static_cast<int>(previousSize);
  _buf.append(data, len);
  _bytes_read      = static_cast<int>(_buf.size());
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
    return {len, ParseStatus::FAILED};
  }

  // Incomplete input is internal parser state, not an application event.
  if (result == -2) {
    if (_buf.size() > HTTP_BUFSIZ) {
      _failed = true;
      if (_callbacks.onError) {
        _callbacks.onError(ParseError::REQUEST_TOO_LARGE);
      }
      return {len, ParseStatus::FAILED};
    }
    return {len, ParseStatus::NEED_MORE};
  }

  const auto headerSize = static_cast<std::size_t>(result);
  const auto consumed   = headerSize > previousSize
                              ? std::min(len, headerSize - previousSize)
                              : std::size_t{0};
  if (headerSize > HTTP_BUFSIZ) {
    _failed = true;
    if (_callbacks.onError) {
      _callbacks.onError(ParseError::REQUEST_TOO_LARGE);
    }
    return {consumed, ParseStatus::FAILED};
  }
  _buf.resize(headerSize);
  _bytes_read = static_cast<int>(headerSize);

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
  return {consumed, ParseStatus::COMPLETE};
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
