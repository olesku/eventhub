#ifndef EVENTHUB_HTTP_REQUEST_H
#define EVENTHUB_HTTP_REQUEST_H

#include "picohttpparser.h"
#include <map>
#include <string>

using namespace std;

namespace eventhub {
class HTTPRequest {
#define BUFSIZ 8192
#define HTTP_POST_MAX 8192000
#define HTTP_REQUEST_MAX_HEADERS 100

public:
  enum RequestState {
    HTTP_REQ_FAILED,
    HTTP_REQ_INCOMPLETE,
    HTTP_REQ_TO_BIG,
    HTTP_REQ_POST_START,
    HTTP_REQ_POST_INVALID_LENGTH,
    HTTP_REQ_POST_INCOMPLETE,
    HTTP_REQ_POST_TOO_LARGE,
    HTTP_REQ_POST_OK,
    HTTP_REQ_OK
  };

  HTTPRequest();
  ~HTTPRequest();
  RequestState parse(const char* data, int len);
  const string& getPath();
  const string& getMethod();
  const map<string, string>& getHeaders();
  const string getHeader(string header);
  const string getQueryString(string param);
  size_t numQueryString();
  const string& getPostData();
  const string& getErrorMessage();

  inline RequestState getState() { return _state; };

private:
  int _http_minor_version;
  string _buf;
  int _bytes_read;
  int _bytes_read_prev;
  int _post_expected_size;
  int _post_bytes_read;
  bool _is_complete;
  bool _is_post;
  RequestState _state;

  const char *_phr_method, *_phr_path;
  struct phr_header _phr_headers[HTTP_REQUEST_MAX_HEADERS];
  size_t _phr_num_headers, _phr_method_len, _phr_path_len;
  int _phr_minor_version;

  string _path;
  string _method;
  string _post_data;
  string _error_message;
  map<string, string> _headers;
  map<string, string> _query_parameters;
  map<string, string> _qsmap;
  size_t _parse_query_string(const std::string& buf);
  inline RequestState _set_state(RequestState newState) {
    _state = newState;
    return newState;
  };
};
} // namespace eventhub

#endif
