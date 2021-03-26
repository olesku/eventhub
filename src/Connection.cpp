#include "Connection.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>

#include <ctime>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Common.hpp"
#include "Config.hpp"
#include "Server.hpp"
#include "ConnectionWorker.hpp"
#include "Topic.hpp"
#include "TopicManager.hpp"
#include "http/Parser.hpp"
#include "websocket/Parser.hpp"
#include "SSL.hpp"
#include "Util.hpp"

namespace eventhub {
using namespace std;


Connection::Connection(int fd, struct sockaddr_in* csin, Server* server, Worker* worker) : _fd(fd), _server(server), _worker(worker) {
  _is_shutdown = false;
  _is_shutdown_after_flush = false;
  _is_ssl = false;
  _ssl_handshake_retries = 0;

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

  // Initialize SSL if required.
  if (_server->isSSL()) {
    _initSSL();
  }

  _read_buffer.resize(NET_READ_BUFFER_SIZE);
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

void Connection::_initSSL() {
  _ssl = OpenSSLUniquePtr<SSL>(SSL_new(_server->getSSLContext()));
  SSL_set_fd(_ssl.get(), _fd);
  SSL_set_accept_state(_ssl.get());
  _is_ssl = true;
}

void Connection::_doSSLHandshake() {
  ERR_clear_error();
  int ret = SSL_accept(_ssl.get());

  if (_ssl_handshake_retries >= SSL_MAX_HANDSHAKE_RETRY) {
    LOG->error("Max SSL retries ({}) reached for client {}", _ssl_handshake_retries, getIP());
    shutdown();
    return;
  }

  if (ret <= 0) {
    int errorCode = SSL_get_error(_ssl.get(), ret);
    if (errorCode == SSL_ERROR_WANT_READ  ||
        errorCode == SSL_ERROR_WANT_WRITE) {
      LOG->trace("OpenSSL retry handshake. Try #{}", _ssl_handshake_retries);
    } else {
      LOG->trace("Fatal error in SSL_accept: {} for client {}", Util::getSSLErrorString(errorCode), getIP());
      shutdown();
    }
  }

   _ssl_handshake_retries++;
}

void Connection::read() {
  size_t bytesRead = 0;
  int ret = 0;

  if (_read_buffer.size() > 0) {
    _read_buffer.clear();
  }

  if (_is_ssl) {
    if (!SSL_is_init_finished(_ssl.get())) {
      _doSSLHandshake();
      return;
    }

    do {
      // If more read buffer capacity is required increase it by chunks of NET_READ_BUFFER_SIZE.
      if (bytesRead + NET_READ_BUFFER_SIZE > _read_buffer.capacity()) {
        size_t newCapacity = _read_buffer.capacity() + NET_READ_BUFFER_SIZE;

        // TODO: Max variable should be called something else.
        if (newCapacity > WS_MAX_DATA_FRAME_SIZE + NET_READ_BUFFER_SIZE) {
          LOG->error("Client {} exceeded max buffer size. Disconnecting.", getIP());
          shutdown();
          return;
        }

        // Retain data we have in the _read_buffer and copy it back after call to resize().
        // We have to do this since resize() invalidates existing data.
        vector<char> retain;
        retain.resize(bytesRead);
        memcpy(retain.data(), _read_buffer.data(), bytesRead);

        LOG->trace("Resizing readbuffer for client {} to {}", getIP(), newCapacity);
        _read_buffer.resize(newCapacity);
        memcpy(_read_buffer.data(), retain.data(), bytesRead);
      }

      ret = SSL_read(_ssl.get(), _read_buffer.data()+bytesRead, NET_READ_BUFFER_SIZE);

      if (ret > 0) {
        bytesRead += ret;
        continue;
      }

      int err = SSL_get_error(_ssl.get(), ret);
      if (err == SSL_ERROR_ZERO_RETURN || err == SSL_ERROR_SYSCALL) {
        LOG->error("OpenSSL read error: {} for client {}", Util::getSSLErrorString(ERR_get_error()), getIP());
        shutdown();
        return;
      }
    } while (ret > 0);
  } else {
    bytesRead = ::read(_fd, _read_buffer.data(), NET_READ_BUFFER_SIZE);

    if (bytesRead <= 0) {
      shutdown();
      return;
    }
  }

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

ssize_t Connection::write(const string& data) {
  std::lock_guard<std::mutex> lock(_write_lock);
  int ret = 0;

  if ((_write_buffer.length() + data.length()) > NET_WRITE_BUFFER_MAX) {
    _write_buffer.clear();
    shutdown();
    LOG->error("Client {} exceeded max write buffer size of {}.", getIP(), NET_WRITE_BUFFER_MAX);
    return 0;
  }

  _write_buffer.append(data);
  if (_write_buffer.empty()) {
    return 0;
  }

  if (_is_ssl) {
    unsigned int pcktSize = _write_buffer.length() > NET_READ_BUFFER_SIZE ? NET_READ_BUFFER_SIZE : _write_buffer.length();
    ret = SSL_write(_ssl.get(), _write_buffer.c_str(), pcktSize);

    if (ret > 0) {
      _pruneWriteBuffer(ret);
    } else {
      int err = SSL_get_error(_ssl.get(), ret);

      if (!(err == SSL_ERROR_SYSCALL && (errno == EAGAIN || errno == EWOULDBLOCK)) &&
          err != SSL_ERROR_WANT_WRITE && err != SSL_ERROR_WANT_READ)
      {
        LOG->trace("write: SSL err: {} for client {}", Util::getSSLErrorString(err), getIP());
        shutdown();
        return ret;
      }
    }

    if (_write_buffer.length() > 0) {
      _enableEpollOut();
    } else {
      _disableEpollOut();
    }
  } else {
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
  }

  if (_write_buffer.empty() && _is_shutdown_after_flush) {
    shutdown();
  }

  return ret;
}

ssize_t Connection::flushSendBuffer() {
  return write("");
}

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
