#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <memory>
#include <netinet/tcp.h>
#include <spdlog/logger.h>
#include <string.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <unistd.h>
#include <utility>
#include <vector>

#include "AccessController.hpp"
#include "Common.hpp"
#include "Connection.hpp"
#include "ConnectionWorker.hpp"
#include "Forward.hpp"
#include "Logger.hpp"
#include "Topic.hpp"
#include "TopicManager.hpp"
#include "http/Parser.hpp"
#include "websocket/Parser.hpp"

namespace eventhub {

Connection::Connection(ConnectionId id, std::unique_ptr<Transport> transport, const struct sockaddr_in& address,
                       Worker* worker, Config& cfg, ConnectionCallbacks callbacks) : EventhubBase(cfg), _id(id), _transport(std::move(transport)), _csin(address),
                                                                                     _worker(worker), _callbacks(std::move(callbacks)) {
  _is_shutdown_after_flush = false;

  int flag = 1;

  // Set socket to non-blocking.
  fcntl(socketFd(), F_SETFL, O_NONBLOCK);

  // Set KEEPALIVE on socket.
  setsockopt(socketFd(), SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char*>(&flag), sizeof(int));

// If we have TCP_USER_TIMEOUT set it to 10 seconds.
#ifdef TCP_USER_TIMEOUT
  int timeout = 10000;
  setsockopt(socketFd(), SOL_TCP, TCP_USER_TIMEOUT, reinterpret_cast<char*>(&timeout), sizeof(timeout));
#endif

  // Set TCP_NODELAY on socket.
  setsockopt(socketFd(), IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&flag), sizeof(int));

  LOG->trace("Client {} connected.", getIP());

  http::ParserCallbacks httpCallbacks;
  httpCallbacks.onRequest = [this](const http::Request& request) {
    if (_callbacks.onHttpRequest) {
      _callbacks.onHttpRequest(*this, request);
    }
  };
  httpCallbacks.onError = [this](http::ParseError error) {
    if (_callbacks.onHttpError) {
      _callbacks.onHttpError(*this, error);
    }
  };
  _http_parser = std::make_unique<http::Parser>(std::move(httpCallbacks));

  websocket::ParserCallbacks websocketCallbacks;
  websocketCallbacks.onMessage = [this](websocket::FrameType frameType, const std::string& data) {
    if (_callbacks.onWebSocketMessage) {
      _callbacks.onWebSocketMessage(*this, frameType, data);
    }
  };
  websocketCallbacks.onError = [this](websocket::ParserError error) {
    if (_callbacks.onWebSocketError) {
      _callbacks.onWebSocketError(*this, error);
    }
  };
  _websocket_parser  = std::make_unique<websocket::Parser>(MAX_DATA_FRAME_SIZE, std::move(websocketCallbacks));
  _access_controller = std::make_unique<AccessController>(cfg);

  _read_buffer.resize(NET_READ_BUFFER_SIZE);
}

Connection::~Connection() {
  LOG->trace("Client {} disconnected.", getIP());

  if (_queued_bytes != 0) {
    _worker->removeQueuedOutputBytes(_queued_bytes);
  }
  if (_congested) {
    _worker->setConnectionCongested(false);
  }
  unsubscribeAll();
}

void Connection::_updateCongestion() {
  if (!_congested && _queued_bytes >= NET_WRITE_BUFFER_HIGH) {
    _congested = true;
    _worker->setConnectionCongested(true);
  } else if (_congested && _queued_bytes <= NET_WRITE_BUFFER_LOW) {
    _congested = false;
    _worker->setConnectionCongested(false);
  }
}

/**
 * Add EPOLLOUT to the list of monitored events for this client.
 */
void Connection::_enableEpollOut() {
  if (_worker->getEpollFileDescriptor() != -1 && !(_epoll_event.events & EPOLLOUT)) {
    _epoll_event.events |= EPOLLOUT;
    epoll_ctl(_worker->getEpollFileDescriptor(), EPOLL_CTL_MOD, socketFd(), &_epoll_event);
  }
}

/**
 * Remove EPOLLOUT from the list of monitored events for this client.
 */
void Connection::_disableEpollOut() {
  if (_worker->getEpollFileDescriptor() != -1 && (_epoll_event.events & EPOLLOUT)) {
    _epoll_event.events &= ~EPOLLOUT;
    epoll_ctl(_worker->getEpollFileDescriptor(), EPOLL_CTL_MOD, socketFd(), &_epoll_event);
  }
}

void Connection::_setTransportWait(IoWait wait, PendingIo operation) {
  _pending_io   = wait == IoWait::NONE ? PendingIo::NONE : operation;
  _pending_wait = wait;
  if (wait == IoWait::WRITE) {
    _enableEpollOut();
  } else if (wait == IoWait::READ) {
    _disableEpollOut();
  }
}

bool Connection::_advanceHandshake() {
  if (_transport->ready()) {
    if (_pending_io == PendingIo::HANDSHAKE) {
      _pending_io   = PendingIo::NONE;
      _pending_wait = IoWait::NONE;
    }
    return true;
  }
  const auto result = _transport->handshake();
  if (result.status == IoStatus::ERROR || result.status == IoStatus::EOF_REACHED) {
    close();
    return false;
  }
  _setTransportWait(result.wait, PendingIo::HANDSHAKE);
  if (_transport->ready()) {
    _pending_io   = PendingIo::NONE;
    _pending_wait = IoWait::NONE;
  }
  return _transport->ready();
}

void Connection::handleEvents(std::uint32_t events) {
  if (isShutdown()) {
    return;
  }

  bool handledRead  = false;
  bool handledWrite = false;
  if (_pending_io == PendingIo::HANDSHAKE && (events & (EPOLLIN | EPOLLOUT))) {
    if (!_advanceHandshake()) {
      return;
    }
  } else if (_pending_io == PendingIo::WRITE &&
             ((_pending_wait == IoWait::READ && (events & EPOLLIN)) ||
              (_pending_wait == IoWait::WRITE && (events & EPOLLOUT)))) {
    flushSendBuffer();
    handledWrite = true;
  } else if (_pending_io == PendingIo::READ &&
             ((_pending_wait == IoWait::READ && (events & EPOLLIN)) ||
              (_pending_wait == IoWait::WRITE && (events & EPOLLOUT)))) {
    read();
    handledRead = true;
  }

  if (_pending_io == PendingIo::WRITE || _pending_io == PendingIo::READ ||
      _pending_io == PendingIo::HANDSHAKE) {
    return;
  }

  if (!isShutdown() && (events & EPOLLOUT) && !handledWrite) {
    flushSendBuffer();
  }
  if (!isShutdown() && (events & EPOLLIN) && !handledRead) {
    read();
  }
}

/**
 * Read from client, parse and call the correct handler.
 */
void Connection::read() {
  if (isShutdown()) {
    return;
  }

  // Keep the backing storage intact so _read_buffer.data() never dangles; we
  // previously cleared the vector here, which left ::read() writing through a
  // null pointer on some STL implementations.
  if (!_advanceHandshake()) {
    return;
  }
  do {
    const auto result = _transport->read(_read_buffer.data(), _read_buffer.size());
    if (result.status == IoStatus::PROGRESS) {
      _pending_io   = PendingIo::NONE;
      _pending_wait = IoWait::NONE;
      _parseRequest(result.bytes);
    } else if (result.status == IoStatus::WOULD_BLOCK) {
      _setTransportWait(result.wait, PendingIo::READ);
      return;
    } else {
      close();
      return;
    }
  } while (!isShutdown() && _transport->hasPendingRead());
}

/**
 * Parse the request present in our read buffer and call the correct handler.
 */
void Connection::_parseRequest(std::size_t bytesRead) {
  std::string_view input(_read_buffer.data(), bytesRead);
  while (!input.empty() && !isShutdown()) {
    if (_protocol == ConnectionProtocol::HTTP) {
      const auto result = _http_parser->parse(input.data(), input.size());
      input.remove_prefix(result.consumed);
      _applyPendingProtocol();
      if (_protocol == ConnectionProtocol::HTTP || result.status != http::ParseStatus::COMPLETE) {
        break;
      }
      continue;
    }

    if (_protocol == ConnectionProtocol::WEBSOCKET) {
      const auto result = _websocket_parser->parse(input);
      input.remove_prefix(result.consumed);
      if (result.status != websocket::ParseStatus::ACTIVE || result.consumed == 0) {
        break;
      }
      continue;
    }

    // SSE is server-to-client only. Client application bytes are unsupported.
    LOG->debug("SSE connection {} sent unexpected input, disconnecting.", getIP());
    close();
    break;
  }
}

void Connection::_applyPendingProtocol() {
  if (!_pending_protocol) {
    return;
  }
  _protocol = *_pending_protocol;
  _pending_protocol.reset();
  _http_parser.reset();
}

/**
 * Add data to send buffer and enable EPOLLOUT on the socket.
 */
bool Connection::write(const std::string& data) {
  std::lock_guard<std::mutex> lock(_write_lock);

  if (isShutdown()) {
    return false;
  }

  if (data.length() > NET_WRITE_BUFFER_MAX - _queued_bytes) {
    _worker->removeQueuedOutputBytes(_queued_bytes);
    _write_queue.clear();
    _write_offset = 0;
    _queued_bytes = 0;
    _updateCongestion();
    _worker->recordSlowConsumerClose();
    close();
    LOG->error("Client {} exceeded max write buffer size of {}.", getIP(), NET_WRITE_BUFFER_MAX);
    return false;
  }

  _write_queue.push_back(data);
  _queued_bytes += data.size();
  _worker->addQueuedOutputBytes(data.size());
  _updateCongestion();

  if (!_write_queue.empty()) {
    flushSendBuffer();
  }
  return true;
}

/**
 * Write send buffer to the client.
 * This function is only called when we have an EPOLLOUT event.
 **/
ssize_t Connection::flushSendBuffer() {
  if (isShutdown()) {
    _disableEpollOut();
    return 0;
  }

  if (!_advanceHandshake()) {
    return 0;
  }
  if (_write_queue.empty()) {
    _disableEpollOut();
    return 0;
  }

  ssize_t total = 0;
  for (unsigned writes = 0; writes < 16 && !_write_queue.empty(); ++writes) {
    auto& chunk       = _write_queue.front();
    const auto result = _transport->write(chunk.data() + _write_offset,
                                          chunk.size() - _write_offset);
    if (result.status == IoStatus::PROGRESS) {
      _pending_io   = PendingIo::NONE;
      _pending_wait = IoWait::NONE;
      _write_offset += result.bytes;
      _queued_bytes -= result.bytes;
      _worker->removeQueuedOutputBytes(result.bytes);
      _updateCongestion();
      total += static_cast<ssize_t>(result.bytes);
      if (_write_offset == chunk.size()) {
        _write_queue.pop_front();
        _write_offset = 0;
      }
      continue;
    }
    if (result.status == IoStatus::WOULD_BLOCK) {
      _setTransportWait(result.wait, PendingIo::WRITE);
      break;
    }
    close();
    break;
  }

  if (_write_queue.empty()) {
    _disableEpollOut();
  } else if (_pending_io != PendingIo::WRITE || _pending_wait == IoWait::WRITE) {
    _enableEpollOut();
  }
  if (_write_queue.empty() && _is_shutdown_after_flush) {
    close();
  }

  return total;
}

/**
 * Shut down the connection.
 */
void Connection::close() {
  if (_lifecycle != ConnectionLifecycle::CLOSED) {
    _transport->close();
    _lifecycle = ConnectionLifecycle::CLOSED;
  }
}

/**
 * Shut down the client after all data in our send buffer is succesfully
 * written to the client.
 */
void Connection::closeAfterFlush() {
  if (_lifecycle == ConnectionLifecycle::CLOSED) {
    return;
  }
  _lifecycle = ConnectionLifecycle::DRAINING;
  if (!_drain_timer_started) {
    _drain_timer_started                     = true;
    std::weak_ptr<Connection> weakConnection = getSharedPtr();
    _worker->addTimer(CONNECTION_DRAIN_TIMEOUT_MS, [weakConnection](TimerCtx*) {
      if (auto connection = weakConnection.lock();
          connection && connection->lifecycle() == ConnectionLifecycle::DRAINING) {
        connection->close();
      }
    });
  }
  if (_write_queue.empty()) {
    close();
    return;
  }

  _is_shutdown_after_flush = true;
}

const std::string Connection::getIP() {
  char ip[32];
  inet_ntop(AF_INET, &_csin.sin_addr, reinterpret_cast<char*>(&ip), 32);

  return ip;
}

int Connection::addToEpoll(uint32_t epollEvents, std::uint64_t token) {
  _epoll_event.events   = epollEvents;
  _epoll_event.data.u64 = token;

  int ret = epoll_ctl(_worker->getEpollFileDescriptor(), EPOLL_CTL_ADD, socketFd(), &_epoll_event);

  return ret;
}

int Connection::removeFromEpoll() {
  if (_worker->getEpollFileDescriptor() != -1) {
    return epoll_ctl(_worker->getEpollFileDescriptor(), EPOLL_CTL_DEL, socketFd(), 0);
  }

  return 0;
}

bool Connection::upgradeToWebSocket() {
  if (_lifecycle != ConnectionLifecycle::OPEN || _protocol != ConnectionProtocol::HTTP || _pending_protocol) {
    return false;
  }
  _pending_protocol = ConnectionProtocol::WEBSOCKET;
  return true;
}

bool Connection::startEventStream() {
  if (_lifecycle != ConnectionLifecycle::OPEN || _protocol != ConnectionProtocol::HTTP || _pending_protocol) {
    return false;
  }
  _pending_protocol = ConnectionProtocol::SSE;
  return true;
}

void Connection::subscribe(const std::string& topicPattern, const jsonrpcpp::Id subscriptionRequestId) {
  std::lock_guard<std::mutex> lock(_subscription_list_lock);
  auto tm = _worker->getTopicManager();

  if (_subscribedTopics.count(topicPattern)) {
    return;
  }

  auto topicSubscription = tm->subscribeConnection(getSharedPtr(), topicPattern, subscriptionRequestId);
  _subscribedTopics.insert(std::make_pair(topicPattern, TopicSubscription{topicSubscription.first, topicSubscription.second, subscriptionRequestId}));
}

AccessController* Connection::getAccessController() {
  return _access_controller.get();
}

ConnectionPtr Connection::getSharedPtr() {
  return shared_from_this();
}

bool Connection::unsubscribe(const std::string& topicPattern) {
  std::lock_guard<std::mutex> lock(_subscription_list_lock);
  auto tm = _worker->getTopicManager();

  if (_subscribedTopics.count(topicPattern) == 0) {
    return false;
  }

  auto it            = _subscribedTopics.find(topicPattern);
  auto& subscription = it->second;

  subscription.topic->deleteSubscriber(subscription.subscriptionId);

  if (subscription.topic->getSubscriberCount() == 0) {
    tm->deleteTopic(topicPattern, subscription.topic);
  }

  _subscribedTopics.erase(it);

  return true;
}

std::size_t Connection::unsubscribeAll() {
  std::lock_guard<std::mutex> lock(_subscription_list_lock);
  auto tm    = _worker->getTopicManager();
  auto count = _subscribedTopics.size();

  for (auto it = _subscribedTopics.begin(); it != _subscribedTopics.end();) {
    auto& subscription = it->second;
    subscription.topic->deleteSubscriber(subscription.subscriptionId);

    if (subscription.topic->getSubscriberCount() == 0) {
      tm->deleteTopic(it->first, subscription.topic);
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
