#include <string.h>
#include <thread>
#include <sys/epoll.h>
#include <functional>

namespace eventhub {
  using redis_subscriber_callback = std::function<void(const std::string& topic, const std::string& data)>;
  class redis_subscriber {

    typedef enum {
      READ_COMMAND,
      READ_ARRAY,
      READ_BULK_STRING
    } PARSER_STATE;

    typedef enum {
      REDIS_SIMPLE_STRING,
      REDIS_ERROR,
      REDIS_INTEGER,
      REDIS_BULK_STRING,
      REDIS_ARRAY
    } REDIS_DATA_TYPE;

    private:
      std::thread _thread;
      int _fd, _epoll_fd;

      std::string _host;
      unsigned int _port;
      bool _connected;

      redis_subscriber_callback _callback;
      PARSER_STATE _state;

      size_t _expected_bulk_len;

      std::string _parse_buffer;
      std::string _bulk_buffer;

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
  };
}
