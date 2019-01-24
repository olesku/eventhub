#ifndef EVENTHUB_CONNECTION_HPP
#define EVENTHUB_CONNECTION_HPP

#include <string>
#include <mutex>
#include <memory>
#include <unordered_map>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <stdint.h>
#include "connection.hpp"

using namespace std;

namespace eventhub {
  class connection {
    public:
      connection(int fd, struct sockaddr_in* csin);
      ~connection();
      ssize_t write(const string &data);
      ssize_t read(void* buf, size_t len);
      ssize_t flush_send_buffer();
      int get_fd();
      const string get_ip();
      int add_to_epoll(int epoll_fd, uint32_t events);
    
    private:
      int _fd;
      struct sockaddr_in _csin;
      struct epoll_event _epoll_event;
      int   _epoll_fd;
      string _write_buffer;
      std::mutex _write_lock;
      
      void _enable_epoll_out();
      void _disable_epoll_out();
      size_t _prune_write_buffer(size_t bytes);
  };

  typedef std::unordered_map<unsigned int, std::shared_ptr<eventhub::connection> > connection_list;
}

#endif
