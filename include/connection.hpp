#ifndef EVENTHUB_CONNECTION_HPP
#define EVENTHUB_CONNECTION_HPP

#include <string>
#include <mutex>
#include <memory>
#include <ctime>
#include <unordered_map>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <stdint.h>
#include "http_request.hpp"

using namespace std;

namespace eventhub {
  class connection {
    public:
      enum state {
        HTTP_PARSE,
        HTTP_PARSE_FAILED,
        HTTP_PARSE_OK,

        WS_PARSE,
        WS_PARSE_FAILED,
        WS_PARSE_OK
      };

      connection(int fd, struct sockaddr_in* csin);
      ~connection();
      ssize_t write(const string &data);
      ssize_t read(char *buf, size_t bytes);
      ssize_t flush_send_buffer();
      int get_fd();
      const string get_ip();
      int add_to_epoll(int epoll_fd, uint32_t events);

      inline void set_state(state new_state)  { _state = new_state; };
      inline const state get_state()          { return _state; };
      inline http_request& get_http_request() { return _http_request; }
    
    private:
      int _fd;
      struct sockaddr_in _csin;
      struct epoll_event _epoll_event;
      int   _epoll_fd;
      string _write_buffer;
      std::mutex _write_lock;
      http_request _http_request;
      state _state;
      time_t _connect_timestamp;
      
      void _enable_epoll_out();
      void _disable_epoll_out();
      size_t _prune_write_buffer(size_t bytes);
  };

  typedef std::unordered_map<unsigned int, std::shared_ptr<eventhub::connection> > connection_list;
}

#endif
