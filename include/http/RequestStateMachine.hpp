#ifndef EVENTHUB_HTTP_REQUESTSTATEMACHINE_H
#define EVENTHUB_HTTP_REQUESTSTATEMACHINE_H

#include "http/picohttpparser.h"
#include <map>
#include <memory>
#include <string>

namespace eventhub {
namespace http {

using namespace std;

enum class State {
  REQ_FAILED,
  REQ_INCOMPLETE,
  REQ_TO_BIG,
  REQ_POST_START,
  REQ_POST_INVALID_LENGTH,
  REQ_POST_INCOMPLETE,
  REQ_POST_TOO_LARGE,
  REQ_POST_OK,
  REQ_OK
};

class RequestStateMachine {
#define HTTP_BUFSIZ 8192
#define HTTP_POST_MAX 8192000
#define HTTP_REQUEST_MAX_HEADERS 100

public:
  RequestStateMachine();
  ~RequestStateMachine();
  State process(const char* data, int len);
  const string& getPath();
  const string& getMethod();
  const map<string, string>& getHeaders();
  const string getHeader(string header);
  const string getQueryString(string param);
  size_t numQueryString();
  const string& getPostData();
  const string& getErrorMessage();

  inline State getState() { return _state; }

private:
  int _http_minor_version;
  string _buf;
  int _bytes_read;
  int _bytes_read_prev;
  int _post_expected_size;
  int _post_bytes_read;
  bool _is_complete;
  bool _is_post;
  State _state;
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

  inline State _set_state(State newState) {
    _state = newState;
    return newState;
  }
};

using RequestStateMachinePtr = std::shared_ptr<RequestStateMachine>;
} // namespace http
} // namespace eventhub

#endif