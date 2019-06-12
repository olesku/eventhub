#include <string.h>
#include <thread>
#include <sys/epoll.h>
#include <functional>

namespace eventhub {
  using redis_subscriber_callback = std::function<void(const std::string& topic, const std::string& data)>;
  class redis_subscriber {

    enum REDIS_PARSER_STATE {
      EXPECT_ARRAY
    };

    private:
      std::thread _thread;
      int _fd, _epoll_fd;
      
      std::string _host;
      unsigned int _port;
      bool _connected;

      redis_subscriber_callback _callback;

      void thread_main();
      void process_statement(const std::string stmt);

    public:
      redis_subscriber();
      ~redis_subscriber();

      bool connect(const std::string& host, const std::string& port);
      void disconnect();
      void set_callback(redis_subscriber_callback callback);
      void psubscribe(const std::string& pattern);
      void parse(const char* data, ssize_t len);
  };
}
