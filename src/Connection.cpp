#include "Connection.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <ctime>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Common.hpp"
#include "Config.hpp"
#include "ConnectionWorker.hpp"
#include "Topic.hpp"
#include "TopicManager.hpp"
#include "http/Parser.hpp"
#include "websocket/Parser.hpp"

namespace eventhub {
using namespace std;

Connection::Connection(int fd, struct sockaddr_in* csin, Worker* worker) : _fd(fd), _worker(worker) {
  _is_shutdown = false;

  memcpy(&_csin, csin, sizeof(struct sockaddr_in));
  int flag = 1;

  // Set socket to non-blocking.
  fcntl(fd, F_SETFL, O_NONBLOCK);

  // Set KEEPALIVE on socket.
  setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char*>(&flag), sizeof(int));

// If we have TCP_USER_TIMEOUT set it to 10 seconds.
#ifdef TCP_USER_TIMEOUT
  int timeout = 10000;
  setsockopt(fd, SOL_TCP, TCP_USER_TIMEOUT, reinterpret_cast<char*>(&timeout), sizeof(timeout));
#endif

  // Set TCP_NODELAY on socket.
  setsockopt(_fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&flag), sizeof(int));

  LOG->trace("Client {} connected.", getIP());

  _http_parser = std::make_unique<http::Parser>();

  // Set initial state.
  setState(ConnectionState::HTTP);
}

Connection::~Connection() {
  LOG->trace("Client {} disconnected.", getIP());

  if (_worker->getEpollFileDescriptor() != -1) {
    epoll_ctl(_worker->getEpollFileDescriptor(), EPOLL_CTL_DEL, _fd, 0);
  }

  close(_fd);
  unsubscribeAll();
}

void Connection::_enableEpollOut() {
  if (_worker->getEpollFileDescriptor() != -1 && !(_epoll_event.events & EPOLLOUT)) {
    _epoll_event.events |= EPOLLOUT;
    epoll_ctl(_worker->getEpollFileDescriptor(), EPOLL_CTL_MOD, _fd, &_epoll_event);
  }
}

void Connection::_disableEpollOut() {
  if (_worker->getEpollFileDescriptor() != -1 && (_epoll_event.events & EPOLLOUT)) {
    _epoll_event.events &= ~EPOLLOUT;
    epoll_ctl(_worker->getEpollFileDescriptor(), EPOLL_CTL_MOD, _fd, &_epoll_event);
  }
}

size_t Connection::_pruneWriteBuffer(size_t bytes) {
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

void Connection::read() {
  char buf[NET_READ_BUFFER_SIZE];
  ssize_t bytesRead = ::read(_fd, buf, NET_READ_BUFFER_SIZE);

  if (bytesRead > 0) {
    buf[bytesRead] = '\0';
  } else {
    buf[0] = '\0';
  }

  if (bytesRead < 1) {
    shutdown();
    return;
  }

  // _parser.parse(buf, bytesRead);
  // Redirect request to either HTTP handler or websocket handler
  // based on which state the client is in.
  switch (getState()) {
    case ConnectionState::HTTP:
      _http_parser->parse(buf, bytesRead);
      break;

    case ConnectionState::WEBSOCKET:
      _websocket_parser.parse(buf, bytesRead);
      break;

    default:
      LOG->debug("Connection {} has invalid state, disconnecting.", getIP());
      shutdown();
  }
}

ssize_t Connection::write(const string& data) {
  std::lock_guard<std::mutex> lock(_write_lock);
  int ret = 0;

  _write_buffer.append(data);
  if (_write_buffer.empty())
    return 0;

  ret = ::write(_fd, _write_buffer.c_str(), _write_buffer.length());

  if (ret <= 0) {
    LOG->trace("Client {} write error: {}.", getIP(), strerror(errno));
    _enableEpollOut();
  } else if ((unsigned int)ret < _write_buffer.length()) {
    LOG->trace("Client {} could not write() entire buffer, wrote {} of {} bytes.", ret, _write_buffer.length());
    _pruneWriteBuffer(ret);
    _enableEpollOut();
  } else {
    _disableEpollOut();
    _write_buffer.clear();
  }

  return ret;
}

ssize_t Connection::flushSendBuffer() {
  return write("");
}

const std::string Connection::getIP() {
  char ip[32];
  inet_ntop(AF_INET, &_csin.sin_addr, reinterpret_cast<char*>(&ip), 32);

  return ip;
}

int Connection::addToEpoll(ConnectionListIterator connectionIterator, uint32_t epollEvents) {
  _connection_list_iterator = connectionIterator;

  _epoll_event.events   = epollEvents;
  _epoll_event.data.fd  = _fd;
  _epoll_event.data.ptr = reinterpret_cast<void*>(this);

  int ret = epoll_ctl(_worker->getEpollFileDescriptor(), EPOLL_CTL_ADD, _fd, &_epoll_event);

  return ret;
}

ConnectionState Connection::setState(ConnectionState newState) {
  if (newState == ConnectionState::WEBSOCKET && _http_parser.get()) {
    _http_parser.reset();
  }

  _state = newState;
  return newState;
}

void Connection::subscribe(const std::string& topicPattern, const jsonrpcpp::Id subscriptionRequestId) {
  std::lock_guard<std::mutex> lock(_subscription_list_lock);
  auto& tm = _worker->getTopicManager();

  if (_subscribedTopics.count(topicPattern)) {
    return;
  }

  auto topicSubscription = tm.subscribeConnection(shared_from_this(), topicPattern, subscriptionRequestId);
  _subscribedTopics.insert(std::make_pair(topicPattern, TopicSubscription{topicSubscription.first, topicSubscription.second, subscriptionRequestId}));
}

ConnectionState Connection::getState() {
  return _state;
}

void Connection::onWebsocketRequest(websocket::ParserCallback callback) {
  _websocket_parser.setCallback(callback);
}

void Connection::onHTTPRequest(http::ParserCallback callback) {
  _http_parser->setCallback(callback);
}

AccessController& Connection::getAccessController() {
  return _access_controller;
}

ConnectionListIterator Connection::getConnectionListIterator() {
  return _connection_list_iterator;
}

ConnectionPtr Connection::getSharedPtr() {
  return shared_from_this();
}

bool Connection::unsubscribe(const std::string& topicPattern) {
  std::lock_guard<std::mutex> lock(_subscription_list_lock);
  auto& tm = _worker->getTopicManager();

  if (_subscribedTopics.count(topicPattern) == 0) {
    return false;
  }

  auto it            = _subscribedTopics.find(topicPattern);
  auto& subscription = it->second;

  subscription.topic->deleteSubscriberByIterator(subscription.topicListIterator);
  _subscribedTopics.erase(it);

  if (subscription.topic->getSubscriberCount() == 0) {
    tm.deleteTopic(topicPattern);
  }

  return true;
}

unsigned int Connection::unsubscribeAll() {
  std::lock_guard<std::mutex> lock(_subscription_list_lock);
  auto& tm           = _worker->getTopicManager();
  unsigned int count = _subscribedTopics.size();

  // TODO: If erase fails here we might end up in a infinite loop.
  for (auto it = _subscribedTopics.begin(); it != _subscribedTopics.end();) {
    auto& subscription = it->second;
    subscription.topic->deleteSubscriberByIterator(subscription.topicListIterator);

    if (subscription.topic->getSubscriberCount() == 0) {
      tm.deleteTopic(it->first);
    }

    it = _subscribedTopics.erase(it);
  }

  return count;
}

std::vector<std::string> Connection::listSubscriptions() {
  std::lock_guard<std::mutex> lock(_subscription_list_lock);
  std::vector<std::string> subscriptionList;

  for (auto& topic : _subscribedTopics) {
    subscriptionList.push_back(topic.first);
  }

  return subscriptionList;
}

} // namespace eventhub
