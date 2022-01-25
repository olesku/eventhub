#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <spdlog/logger.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Connection.hpp"
#include "Common.hpp"
#include "ConnectionWorker.hpp"
#include "Topic.hpp"
#include "TopicManager.hpp"
#include "http/Parser.hpp"
#include "websocket/Parser.hpp"
#include "Logger.hpp"

namespace eventhub {
class Config;

Connection::Connection(int fd, struct sockaddr_in* csin, Worker* worker, Config& cfg) :
  EventhubBase(cfg), _fd(fd), _worker(worker), _access_controller(cfg) {

  _is_shutdown             = false;
  _is_shutdown_after_flush = false;

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

  _read_buffer.resize(NET_READ_BUFFER_SIZE);
}

Connection::~Connection() {
  LOG->trace("Client {} disconnected.", getIP());

  close(_fd);
  unsubscribeAll();
}

/**
 * Add EPOLLOUT to the list of monitored events for this client.
 */
void Connection::_enableEpollOut() {
  if (_worker->getEpollFileDescriptor() != -1 && !(_epoll_event.events & EPOLLOUT)) {
    _epoll_event.events |= EPOLLOUT;
    epoll_ctl(_worker->getEpollFileDescriptor(), EPOLL_CTL_MOD, _fd, &_epoll_event);
  }
}

/**
 * Remove EPOLLOUT from the list of monitored events for this client.
 */
void Connection::_disableEpollOut() {
  if (_worker->getEpollFileDescriptor() != -1 && (_epoll_event.events & EPOLLOUT)) {
    _epoll_event.events &= ~EPOLLOUT;
    epoll_ctl(_worker->getEpollFileDescriptor(), EPOLL_CTL_MOD, _fd, &_epoll_event);
  }
}

/**
 * Remove n bytes from the beginning og the write buffer.
 */
size_t Connection::_pruneWriteBuffer(size_t bytes) {
  if (_write_buffer.length() < 1) {
    return 0;
  }

  if (bytes >= _write_buffer.length()) {
    _write_buffer.clear();
    return 0;
  }

  _write_buffer = _write_buffer.substr(bytes, std::string::npos);
  return _write_buffer.length();
}

/**
 * Read from client, parse and call the correct handler.
 */
void Connection::read() {
  _read_buffer.clear();

  if (isShutdown()) {
    return;
  }

  size_t bytesRead = 0;
  bytesRead        = ::read(_fd, _read_buffer.data(), NET_READ_BUFFER_SIZE);

  if (bytesRead <= 0) {
    if (errno != EAGAIN) {
      shutdown();
    }

    return;
  }

  _parseRequest(bytesRead);
}

/**
 * Parse the request present in our read buffer and call the correct handler.
 */
void Connection::_parseRequest(size_t bytesRead) {
  // Redirect request to either HTTP handler or websocket handler
  // based on which state the client is in.
  switch (getState()) {
    case ConnectionState::HTTP:
      _http_parser->parse(_read_buffer.data(), bytesRead);
      break;

    case ConnectionState::WEBSOCKET:
      _websocket_parser.parse(_read_buffer.data(), bytesRead);
      break;

    default:
      LOG->debug("Connection {} has invalid state, disconnecting.", getIP());
      shutdown();
  }

  _read_buffer.clear();
}

/**
 * Add data to send buffer and enable EPOLLOUT on the socket.
 */
void Connection::write(const std::string& data) {
  std::lock_guard<std::mutex> lock(_write_lock);

  if (isShutdown()) {
    return;
  }

  if ((_write_buffer.length() + data.length()) > NET_WRITE_BUFFER_MAX) {
    _write_buffer.clear();
    shutdown();
    LOG->error("Client {} exceeded max write buffer size of {}.", getIP(), NET_WRITE_BUFFER_MAX);
    return;
  }

  _write_buffer.append(data);

  if (!_write_buffer.empty()) {
    flushSendBuffer();
  }
}

/**
 * Write send buffer to the client.
 * This function is only called when we have an EPOLLOUT event.
 **/
ssize_t Connection::flushSendBuffer() {
  if (_write_buffer.empty() || isShutdown()) {
    _disableEpollOut();
    return 0;
  }

  int ret = ::write(_fd, _write_buffer.c_str(), _write_buffer.length());

  if (ret <= 0) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      LOG->trace("Client {} write error: {}.", getIP(), strerror(errno));
      shutdown();
    } else {
      _enableEpollOut();
    }
  } else if ((unsigned int)ret < _write_buffer.length()) {
    LOG->trace("Client {} could not write() entire buffer, wrote {} of {} bytes.", ret, _write_buffer.length());
    _pruneWriteBuffer(ret);
    _enableEpollOut();
  } else {
    _disableEpollOut();
    _write_buffer.clear();
  }

  if (_write_buffer.empty() && _is_shutdown_after_flush) {
    shutdown();
  }

  return ret;
}

/**
 * Shut down the connection.
 */
void Connection::shutdown() {
  if (!_is_shutdown) {
    ::shutdown(_fd, SHUT_RDWR);
    _is_shutdown = true;
  }
}

/**
 * Shut down the client after all data in our send buffer is succesfully
 * written to the client.
 */
void Connection::shutdownAfterFlush() {
  if (_write_buffer.empty()) {
    shutdown();
    return;
  }

  _is_shutdown_after_flush = true;
}

const std::string Connection::getIP() {
  char ip[32];
  inet_ntop(AF_INET, &_csin.sin_addr, reinterpret_cast<char*>(&ip), 32);

  return ip;
}

int Connection::addToEpoll(uint32_t epollEvents) {
  _epoll_event.events   = epollEvents;
  _epoll_event.data.fd  = _fd;
  _epoll_event.data.ptr = reinterpret_cast<void*>(this);

  int ret = epoll_ctl(_worker->getEpollFileDescriptor(), EPOLL_CTL_ADD, _fd, &_epoll_event);

  return ret;
}

int Connection::removeFromEpoll() {
  if (_worker->getEpollFileDescriptor() != -1) {
    return epoll_ctl(_worker->getEpollFileDescriptor(), EPOLL_CTL_DEL, _fd, 0);
  }

  return 0;
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

  auto topicSubscription = tm.subscribeConnection(getSharedPtr(), topicPattern, subscriptionRequestId);
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

void Connection::assignConnectionListIterator(std::list<ConnectionPtr>::iterator connectionIterator) {
  _connection_list_iterator = connectionIterator;
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

  if (subscription.topic->getSubscriberCount() == 0) {
    tm.deleteTopic(topicPattern);
  }

  _subscribedTopics.erase(it);

  return true;
}

unsigned int Connection::unsubscribeAll() {
  std::lock_guard<std::mutex> lock(_subscription_list_lock);
  auto& tm           = _worker->getTopicManager();
  unsigned int count = _subscribedTopics.size();

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
