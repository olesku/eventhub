#ifndef EVENTHUB_CONNECTION_HPP
#define EVENTHUB_CONNECTION_HPP

#include "AccessController.hpp"
#include "http/RequestStateMachine.hpp"
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
class Worker;

enum class ConnectionState {
  HTTP,
  WEBSOCKET
};

class Connection {
public:
  Connection(int fd, struct sockaddr_in* csin, Worker* worker);
  ~Connection();

  ssize_t write(const string& data);
  ssize_t read(char* buf, size_t bytes);
  ssize_t flushSendBuffer();

  int addToEpoll(int epollFd, uint32_t events);

  inline ConnectionState getState() { return _state; };
  inline http::RequestStateMachinePtr& getHttpRequest() { return _http_request; }
  inline websocket::StateMachine& getWsFsm() { return _ws_fsm; }
  inline AccessController& getAccessController() { return _access_controller; }
  inline Worker* getWorker() { return _worker; }
  const string getIP();

  inline ConnectionState setState(ConnectionState newState) { return _state = newState; };

  inline void shutdown() {
    !_is_shutdown && ::shutdown(_fd, SHUT_RDWR);
    _is_shutdown = true;
  };

  inline bool isShutdown() { return _is_shutdown; };

private:
  int _fd;
  struct sockaddr_in _csin;
  Worker *_worker;
  struct epoll_event _epoll_event;
  int _epoll_fd;
  string _write_buffer;
  std::mutex _write_lock;
  http::RequestStateMachinePtr _http_request;
  websocket::StateMachine _ws_fsm;
  AccessController _access_controller;
  ConnectionState _state;
  bool _is_shutdown;

  void _enableEpollOut();
  void _disableEpollOut();
  size_t _pruneWriteBuffer(size_t bytes);
};

using ConnectionPtr = std::shared_ptr<Connection>;
} // namespace eventhub

#endif
