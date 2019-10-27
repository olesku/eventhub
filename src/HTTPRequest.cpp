#include "HTTPRequest.hpp"
#include "Common.hpp"
#include "picohttpparser.h"
#include <iostream>
#include <string.h>

namespace eventhub {

/**
    Constructor.
  **/
HTTPRequest::HTTPRequest() {
  _bytes_read         = 0;
  _bytes_read_prev    = 0;
  _post_expected_size = 0;
  _post_bytes_read    = 0;
  _is_complete        = false;
  _is_post            = false;
  _post_data          = "";
  _error_message      = "";
}

/**
    Destructor.
  **/
HTTPRequest::~HTTPRequest() {}

/**
    Parse the request.
    @param data Raw http request data.
    @param len Length of data.
  **/
HTTPRequest::RequestState HTTPRequest::parse(const char* data, int len) {
  int pret;

  if (_is_post) {
    _post_bytes_read += len;

    if (_post_bytes_read > HTTP_POST_MAX) {
      DLOG(ERROR) << "HTTP_REQ_POST_TOO_LARGE "
                  << "POST data is bigger than " << HTTP_POST_MAX;
      _error_message = "HTTP_REQ_POST_TOO_LARGE: POST data is to large.";
      _post_data.clear();
      return _set_state(HTTP_REQ_POST_TOO_LARGE);
    }

    _post_data.append(data);

    if (_post_bytes_read < _post_expected_size) {
      return _set_state(HTTP_REQ_POST_INCOMPLETE);
    }

    return _set_state(HTTP_REQ_POST_OK);
  }

  if (_is_complete)
    return _set_state(HTTP_REQ_OK);
  _bytes_read_prev = _bytes_read;

  // Request is to large.
  if ((_bytes_read + len) > BUFSIZ) {
    _error_message = "HTTP_REQ_TO_BIG: Request to large.";
    return _set_state(HTTP_REQ_TO_BIG);
  }

  _bytes_read += len;
  _buf.append(data, len);

  _phr_num_headers = sizeof(_phr_headers) / sizeof(_phr_headers[0]);

  pret = phr_parse_request(_buf.c_str(), _bytes_read, &_phr_method, &_phr_method_len, &_phr_path,
                           &_phr_path_len, &_phr_minor_version, _phr_headers, &_phr_num_headers, _bytes_read_prev);

  // Parse error.
  if (pret == -1) {
    DLOG(ERROR) << "HTTP_REQ_FAILED";
    _error_message = "HTTP_REQ_FAILED: Parse failed.";
    return _set_state(HTTP_REQ_FAILED);
  }

  // Request incomplete.
  if (pret == -2) {
    DLOG(INFO) << "HTTP_REQ_INCOMPLETE";
    return _set_state(HTTP_REQ_INCOMPLETE);
  }

  if (_phr_method_len > 0)
    _method.insert(0, _phr_method, _phr_method_len);

  if (_phr_path_len > 0) {
    string rawPath;
    rawPath.insert(0, _phr_path, _phr_path_len);

    size_t qsPos = rawPath.find_first_of('?', 0);
    if (qsPos != string::npos) {
      string qStr;
      qStr  = rawPath.substr(qsPos + 1, string::npos);
      _path = rawPath.substr(0, qsPos);
      _parse_query_string(qStr);
    } else {
      _path = rawPath.substr(0, rawPath.find_last_of(' ', 0));
    }
  }

  for (int i = 0; i < (int)_phr_num_headers; i++) {
    string name, value;
    name.insert(0, _phr_headers[i].name, _phr_headers[i].name_len);
    value.insert(0, _phr_headers[i].value, _phr_headers[i].value_len);
    strTolower(name);
    _headers[name] = value;
  }

  if (getMethod().compare("POST") == 0) {
    if (getHeader("Content-Length").empty()) {
      _error_message = "HTTP_REQ_POST_INVALID_LENGTH: No Content-Length header set.";
      return _set_state(HTTP_REQ_POST_INVALID_LENGTH);
    } else {
      try {
        _post_expected_size = std::stoi(getHeader("Content-Length"));
      } catch (...) {
        _error_message = "HTTP_REQ_POST_INVALID_LENGTH: Invalid format.";
        return _set_state(HTTP_REQ_POST_INVALID_LENGTH);
      }
    }

    if (_post_expected_size < 1) {
      _error_message = "HTTP_REQ_POST_INVAID_LENGTH: Cannot be zero.";
      return _set_state(HTTP_REQ_POST_INVALID_LENGTH);
    }

    _is_post = true;

    // If we have post data in the initial request run Parse on it to take correct action.
    if (len > pret) {
      string tmp;
      tmp.insert(0, data, pret, len - pret);
      return parse(tmp.c_str(), tmp.length());
    }

    return _set_state(HTTP_REQ_POST_START);
  }

  _is_complete      = true;
  _buf[_bytes_read] = '\0';
  DLOG(INFO) << "HTTP_REQ_OK";

  return _set_state(HTTP_REQ_OK);
}

/**
    Get the HTTP request path.
  **/
const string& HTTPRequest::getPath() {
  return _path;
}

/**
    Get the HTTP request method.
  **/
const string& HTTPRequest::getMethod() {
  return _method;
}

/**
    Get a spesific header.
    @param header Header to get.
  **/
const string HTTPRequest::getHeader(string header) {
  strTolower(header);

  if (_headers.find(header) != _headers.end()) {
    return _headers[header];
  }

  return "";
}

const map<string, string>& HTTPRequest::getHeaders() {
  return _headers;
}

/**
    Extracts query parameters from a string if they exist.
    @param buf The string to parse.
  **/
size_t HTTPRequest::_parse_query_string(const std::string& buf) {
  size_t prevpos = 0, eqlpos = 0;

  while ((eqlpos = buf.find("=", prevpos)) != string::npos) {
    string param, val;
    size_t len;

    len = buf.find("&", eqlpos);

    if (len != string::npos)
      len = (len - eqlpos);
    else
      len = (buf.size() - eqlpos);

    param = buf.substr(prevpos, (eqlpos - prevpos));

    eqlpos++;
    val     = buf.substr(eqlpos, len - 1);
    prevpos = eqlpos + len;

    if (!param.empty() && !val.empty()) {
      strTolower(param);
      _qsmap[param] = val;
    }
  }

  return _qsmap.size();
}

/**
    Get a spesific query string parameter.
    @param param Parameter to get.
  **/
const string HTTPRequest::getQueryString(string param) {
  strTolower(param);

  if (_qsmap.find(param) != _qsmap.end()) {
    return _qsmap[param];
  }

  return "";
}

/**
    Returns number of query strings in the request.
  **/
size_t HTTPRequest::numQueryString() {
  return _qsmap.size();
}

const string& HTTPRequest::getPostData() {
  return _post_data;
}

const string& HTTPRequest::getErrorMessage() {
  return _error_message;
}

} // namespace eventhub
