#pragma once

#include <string>

#include "http/Request.hpp"
#include "http/picohttpparser.h"
#include "http/Types.hpp"

namespace eventhub::http {

class Parser final {
#define HTTP_BUFSIZ 8192
#define HTTP_REQUEST_MAX_HEADERS 100

public:
  explicit Parser(ParserCallbacks callbacks = {});
  void parse(const char* data, std::size_t len);

private:
  std::string _buf;
  int _bytes_read;
  int _bytes_read_prev;
  bool _is_complete;
  bool _failed;
  const char *_phr_method, *_phr_path;
  struct phr_header _phr_headers[HTTP_REQUEST_MAX_HEADERS];
  std::size_t _phr_num_headers, _phr_method_len, _phr_path_len;
  int _phr_minor_version;
  Request _request;
  ParserCallbacks _callbacks;

  void _resetState();
};

} // namespace eventhub::http
