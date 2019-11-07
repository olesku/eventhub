#ifndef INCLUDE_HTTP_PARSER_HPP_
#define INCLUDE_HTTP_PARSER_HPP_

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "http/picohttpparser.h"

namespace eventhub {
namespace http {

using namespace std;

enum class RequestState {
  REQ_FAILED,
  REQ_INCOMPLETE,
  REQ_TO_BIG,
  REQ_OK
};

using ParserCallback = std::function<void(class Parser* req, RequestState state)>;

class Parser {
#define HTTP_BUFSIZ 8192
#define HTTP_REQUEST_MAX_HEADERS 100

public:
  Parser();
  ~Parser();
  void parse(const char* data, int len);
  const string& getPath();
  const string& getMethod();
  const map<string, string>& getHeaders();
  const string getHeader(string header);
  const string getQueryString(string param);
  size_t numQueryString();
  const string& getErrorMessage();
  void setCallback(ParserCallback callback);

private:
  int _http_minor_version;
  string _buf;
  int _bytes_read;
  int _bytes_read_prev;
  bool _is_complete;
  const char *_phr_method, *_phr_path;
  struct phr_header _phr_headers[HTTP_REQUEST_MAX_HEADERS];
  size_t _phr_num_headers, _phr_method_len, _phr_path_len;
  int _phr_minor_version;
  string _path;
  string _method;
  string _error_message;
  map<string, string> _headers;
  map<string, string> _query_parameters;
  map<string, string> _qsmap;

  size_t _parse_query_string(const std::string& buf);
  ParserCallback _callback;
};

} // namespace http
} // namespace eventhub

#endif // INCLUDE_HTTP_PARSER_HPP_