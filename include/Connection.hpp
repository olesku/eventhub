#ifndef EVENTHUB_CONNECTION_HPP
#define EVENTHUB_CONNECTION_HPP

#include "AccessController.hpp"
#include "HTTPRequest.hpp"
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
  enum State {
    HTTP_MODE,
    WEBSOCKET_MODE
  };

  Connection(int fd, struct sockaddr_in* csin);
  ~Connection();

  ssize_t write(const string& data);
  ssize_t read(char* buf, size_t bytes);
  ssize_t flushSendBuffer();

  int addToEpoll(int epollFd, uint32_t events);
  int getFileDescriptor();
  const string getIP();

  inline State setState(State newState) { return _state = newState; };
  inline const State getState() { return _state; };
  inline HTTPRequest& getHttpRequest() { return _http_request; }
  inline websocket::StateMachine& getWsFsm() { return _ws_fsm; }
  inline AccessController& getAccessController() { return _access_controller; }

  inline void shutdown() {
    !_is_shutdown && ::shutdown(_fd, SHUT_RDWR);
    _is_shutdown = true;
  };
  inline bool isShutdown() { return _is_shutdown; };

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
  State _state;
  bool _is_shutdown;

  void _enableEpollOut();
  void _disableEpollOut();
  size_t _pruneWriteBuffer(size_t bytes);
};
} // namespace eventhub

#endif
