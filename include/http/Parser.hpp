#pragma once

#include <stddef.h>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "http/picohttpparser.h"
#include "http/Types.hpp"

namespace eventhub {
namespace http {

class Parser final {
#define HTTP_BUFSIZ 8192
#define HTTP_REQUEST_MAX_HEADERS 100

public:
  Parser();
  ~Parser();
  void parse(const char* data, int len);
  const std::string& getPath();
  const std::string& getMethod();
  const std::map<std::string, std::string>& getHeaders();
  const std::string getHeader(std::string header);
  const std::string getQueryString(std::string param);
  size_t numQueryString();
  const std::string& getErrorMessage();
  void setCallback(ParserCallback callback);

private:
  std::string _buf;
  int _bytes_read;
  int _bytes_read_prev;
  bool _is_complete;
  const char *_phr_method, *_phr_path;
  struct phr_header _phr_headers[HTTP_REQUEST_MAX_HEADERS];
  size_t _phr_num_headers, _phr_method_len, _phr_path_len;
  int _phr_minor_version;
  std::string _path;
  std::string _method;
  std::string _error_message;
  std::map<std::string, std::string> _headers;
  std::map<std::string, std::string> _query_parameters;
  std::map<std::string, std::string> _qsmap;

  size_t _parse_query_string(const std::string& buf);
  ParserCallback _callback;
};

} // namespace http
} // namespace eventhub


