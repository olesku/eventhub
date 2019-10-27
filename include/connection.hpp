#ifndef EVENTHUB_CONNECTION_HPP
#define EVENTHUB_CONNECTION_HPP

#include "access_controller.hpp"
#include "http_request.hpp"
#include "websocket/StateMachine.hpp"
#include <ctime>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <stdint.h>
#include <string>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unordered_map>

using namespace std;

namespace eventhub {
class Connection {
public:
  enum state {
    HTTP_MODE,
    WEBSOCKET_MODE
  };

  Connection(int fd, struct sockaddr_in* csin);
  ~Connection();

  ssize_t write(const string& data);
  ssize_t read(char* buf, size_t bytes);
  ssize_t flushSendBuffer();

  int addToEpoll(int epoll_fd, uint32_t events);
  int get_fd();
  const string getIP();

  inline state set_state(state new_state) { return _state = new_state; };
  inline const state get_state() { return _state; };
  inline HTTPRequest& get_http_request() { return _http_request; }
  inline websocket::StateMachine& get_ws_fsm() { return _ws_fsm; }
  inline AccessController& get_access_controller() { return _access_controller; }

  inline void shutdown() {
    !_is_shutdown && ::shutdown(_fd, SHUT_RDWR);
    _is_shutdown = true;
  };
  inline bool is_shutdown() { return _is_shutdown; };

private:
  int _fd;
  struct sockaddr_in _csin;
  struct epoll_event _epoll_event;
  int _epoll_fd;
  string _write_buffer;
  std::mutex _write_lock;
  HTTPRequest _http_request;
  websocket::StateMachine _ws_fsm;
  AccessController _access_controller;
  state _state;
  bool _is_shutdown;

  void _enableEpollOut();
  void _disableEpollOut();
  size_t _pruneWriteBuffer(size_t bytes);
};
} // namespace eventhub

#endif
