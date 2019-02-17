#ifndef EVENTHUB_CONNECTION_HPP
#define EVENTHUB_CONNECTION_HPP

#include <string>
#include <mutex>
#include <memory>
#include <ctime>
#include <unordered_map>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include "http_request.hpp"
#include "websocket_request.hpp"

using namespace std;

namespace eventhub {
  namespace io {
    class connection {
      public:
        enum state {
          HTTP_MODE,
          WEBSOCKET_MODE
        };

        connection(int fd, struct sockaddr_in* csin);
        ~connection();

        ssize_t write(const string &data);
        ssize_t read(char *buf, size_t bytes);
        ssize_t flush_send_buffer();

        int add_to_epoll(int epoll_fd, uint32_t events);
        int get_fd();
        const string get_ip();

        inline state set_state(state new_state) { return _state = new_state; };
        inline const state get_state()          { return _state; };
        inline http_request& get_http_request() { return _http_request; }
        inline websocket_request& get_ws_request() { return _ws_request; }

        inline void shutdown() { !_is_shutdown && ::shutdown(_fd, SHUT_RDWR); _is_shutdown = true; };
        inline bool is_shutdown() { return _is_shutdown; };

      private:
        int _fd;
        struct sockaddr_in _csin;
        struct epoll_event _epoll_event;
        int   _epoll_fd;
        string _write_buffer;
        std::mutex _write_lock;
        http_request _http_request;
        websocket_request _ws_request;
        state _state;
        bool _is_shutdown;
        
        void _enable_epoll_out();
        void _disable_epoll_out();
        size_t _prune_write_buffer(size_t bytes);
    };
  }
}

#endif
