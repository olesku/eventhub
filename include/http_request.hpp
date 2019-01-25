#ifndef EVENTHUB_HTTP_REQUEST_H
#define EVENTHUB_HTTP_REQUEST_H

#include <string>
#include <map>
#include "picohttpparser.h"

using namespace std;

namespace eventhub {
  class http_request {
    #define BUFSIZ 8192
    #define HTTP_POST_MAX 8192000
    #define HTTP_REQUEST_MAX_HEADERS 100

    public:
      enum request_state {
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

      http_request();
      ~http_request();
      request_state parse(const char *data, int len);
      const string& get_path();
      const string& get_method();
      const map<string, string>& get_headers();
      const string get_header(string header);
      const string get_query_string(string param);
      size_t num_query_string();
      const string& get_post_data();
      const string& get_error_message();

      inline request_state get_state() { return _state; };

    private:
      int _http_minor_version;
      string _buf;
      int _bytes_read;
      int _bytes_read_prev;
      int _post_expected_size;
      int _post_bytes_read;
      bool _is_complete;
      bool _is_post;
      request_state _state;

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
      inline request_state _set_state(request_state new_state) { _state = new_state; return new_state; };
  };
}

#endif
