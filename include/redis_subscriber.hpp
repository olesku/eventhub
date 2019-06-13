#include <string.h>
#include <thread>
#include <sys/epoll.h>
#include <vector>
#include <functional>

namespace eventhub {
  using redis_subscriber_callback = std::function<void(const std::string& channel, const std::string& data)>;
  class redis_subscriber {

    typedef enum {
      READ_RESP,
      READ_BULK_STRING
    } PARSER_STATE;

    typedef enum {
      REDIS_SIMPLE_STRING = '+',
      REDIS_ERROR         = '-',
      REDIS_INTEGER       = ':',
      REDIS_BULK_STRING   = '$',
      REDIS_ARRAY         = '*'
    } REDIS_DATA_TYPE;

    typedef struct {
      REDIS_DATA_TYPE type;
      std::string data;
      size_t len;
    } redis_obj_t;

    typedef std::vector<redis_obj_t> redis_array_t;

    private:
      std::thread _thread;
      int _fd, _epoll_fd;

      std::string _host;
      unsigned int _port;
      bool _connected;

      redis_subscriber_callback _callback;
      PARSER_STATE _state;

      size_t _expected_bulk_len;
      size_t _expected_array_len;

      std::string _parse_buffer;
      std::string _bulk_buffer;

      redis_array_t _result_array;

      void thread_main();
      void set_state(PARSER_STATE state) { _state = state; }

    public:
      redis_subscriber();
      ~redis_subscriber();

      bool connect(const std::string& host, const std::string& port);
      void disconnect();
      void set_callback(redis_subscriber_callback callback);
      void psubscribe(const std::string& pattern);
      void parse(const char* data);

      const std::string read_until_crlf();
      const std::string read_bytes(size_t bulk_size);
      void handle_resp(const std::string& data);
      void result_array_push(REDIS_DATA_TYPE type, const std::string& data);
      void handle_pmessage(redis_array_t& data);
      void clear_parser_state();

  };
}
