#include <ctime>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "common.hpp"
#include "connection.hpp"

namespace eventhub {
  namespace io {
    using namespace std;

    connection::connection(int fd, struct sockaddr_in* csin) {
      _fd = fd;
      _epoll_fd = -1;
      _is_shutdown = false;

      memcpy(&_csin, csin, sizeof(struct sockaddr_in));
      int flag = 1;

      // Set socket to non-blocking.
      fcntl(fd, F_SETFL, O_NONBLOCK);

      // Set KEEPALIVE on socket.
      setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&flag, sizeof(int));
    
      // If we have TCP_USER_TIMEOUT set it to 10 seconds.
      #ifdef TCP_USER_TIMEOUT
      int timeout = 10000;
      setsockopt (fd, SOL_TCP, TCP_USER_TIMEOUT, (char*) &timeout, sizeof (timeout));
      #endif

      // Set TCP_NODELAY on socket.
      setsockopt(_fd, IPPROTO_TCP, TCP_NODELAY, (char*)&flag, sizeof(int));

      DLOG(INFO) << "Initialized client with IP: " << get_ip();

      // Set initial state.
      set_state(HTTP_MODE);
    }

    connection::~connection() {
      DLOG(INFO) << "Destructor called for client with IP: " << get_ip();
      
      if (_epoll_fd != -1) {
        epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, _fd, 0);
      }

      close(_fd);
    }

    void connection::_enable_epoll_out() {
      if (_epoll_fd != -1 && !(_epoll_event.events & EPOLLOUT)) {
        _epoll_event.events |= EPOLLOUT;
        epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, _fd, &_epoll_event);
      }
    }

    void connection::_disable_epoll_out() {
      if (_epoll_fd != -1 && (_epoll_event.events & EPOLLOUT)) {
        _epoll_event.events &= ~EPOLLOUT;
        epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, _fd, &_epoll_event);
      }
    }

    size_t connection::_prune_write_buffer(size_t bytes) {
      if (_write_buffer.length() < 1) {
        return 0;
      }

      if (bytes >= _write_buffer.length()) {
        _write_buffer.clear();
        return 0;
      }

      _write_buffer = _write_buffer.substr(bytes, string::npos);
      return _write_buffer.length();
    }

    ssize_t connection::read(char *buf, size_t bytes) {
      ssize_t bytes_read = ::read(_fd, buf, bytes);

      if (bytes_read > 0) {
        buf[bytes_read] = '\0';
      } else {
        buf[0] = '\0';
      }

      return bytes_read;
    }

    ssize_t connection::write(const string &data) {
      std::lock_guard<std::mutex> lock(_write_lock);
      int ret = 0;

      _write_buffer.append(data);
      if (_write_buffer.empty()) return 0;

      ret = ::write(_fd, _write_buffer.c_str(), _write_buffer.length());

      DLOG(INFO) << "write:" << data;

      if (ret <= 0) {
        DLOG(INFO) << get_ip() << ": write error: " << strerror(errno);
        _enable_epoll_out();
      } else if ((unsigned int)ret < _write_buffer.length()) {
        DLOG(INFO) << get_ip() << ": Could not write() entire buffer, wrote " << ret << " of " << _write_buffer.length() << " bytes.";
        _prune_write_buffer(ret);
        _enable_epoll_out();
      } else {
        _disable_epoll_out();
        _write_buffer.clear();
      }

      return ret;
    }

    ssize_t connection::flush_send_buffer() {
      return write("");
    }
    
    int connection::get_fd() {
      return _fd;
    }

    const string connection::get_ip() {
      char ip[32];
      inet_ntop(AF_INET, &_csin.sin_addr, (char*)&ip, 32);

      return ip;
    }

    int connection::add_to_epoll(int epoll_fd, uint32_t events) {
      _epoll_event.events = events;
      _epoll_event.data.fd = _fd;
      int ret = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, _fd, &_epoll_event);
      
      if (ret == 0) {
        _epoll_fd = epoll_fd;
      }

      return ret;
    }
  }
}
