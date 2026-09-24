#include <errno.h>
#include <netinet/in.h>
#include <spdlog/logger.h>
#include <string.h>

#include "ConnectionWorker.hpp"
#include "Logger.hpp"
#include "websocket/Types.hpp"
#ifdef __linux__
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#else
#error "eventhub worker requires Linux (epoll/eventfd/timerfd)"
#endif
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <stdlib.h>
#include <string>
#include <sys/socket.h>
#include <type_traits>
#include <unistd.h>
#include <utility>

#include "Common.hpp"
#include "Config.hpp"
#include "Connection.hpp"
#include "EventLoop.hpp"
#include "HandlerContext.hpp"
#include "Server.hpp"
#include "TopicManager.hpp"
#include "Transport.hpp"
#include "Util.hpp"
#include "http/Handler.hpp"
#include "sse/Response.hpp"
#include "websocket/Handler.hpp"
#include "websocket/Response.hpp"

namespace eventhub {

Worker::Worker(Server* srv, unsigned int workerId) : EventhubBase(srv->config()), _workerId(workerId) {
  _server   = srv;
  _epoll_fd = epoll_create1(0);
  _event_fd = -1;
  _timer_fd = -1;

  _ev            = std::make_unique<EventLoop>();
  _topic_manager = std::make_unique<TopicManager>();

  _initEventFd();
  _initTimerFd();
}

Worker::~Worker() {
  _accepting_commands.store(false, std::memory_order_release);
  _connections.clear();
  {
    std::lock_guard<std::mutex> lock(_pending_connections_mutex);
    _pending_connections.clear();
  }
  if (_epoll_fd != -1) {
    close(_epoll_fd);
  }
  _closeEventFd();
  _closeTimerFd();

  LOG->debug("Connection worker {} shutting down.", getWorkerId());
}

void Worker::addTimer(int64_t delay, std::function<void(TimerCtx* ctx)> callback, bool repeat) {
  _ev->addTimer(delay, callback, repeat);
  _signalWork();
}

bool Worker::enqueueAcceptedSocket(SocketHandle socket, const struct sockaddr_in& address, bool ssl) {
  if (!_accepting_commands.load(std::memory_order_acquire)) {
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(_pending_connections_mutex);
    if (!_accepting_commands.load(std::memory_order_relaxed)) {
      return false;
    }
    _pending_connections.push_back(PendingConnection{std::move(socket), address, ssl});
  }
  _signalWork();
  return true;
}

void Worker::_assertOwnerThread() const {
  assert(_owner_thread == std::this_thread::get_id());
}

void Worker::_wakeForStop() {
  _accepting_commands.store(false, std::memory_order_release);
  _signalWork();
}

void Worker::_initEventFd() {
  _event_fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
  if (_event_fd == -1) {
    LOG->critical("Worker {} failed to create eventfd: {}.", getWorkerId(), strerror(errno));
    exit(1);
  }
}

void Worker::_closeEventFd() {
  if (_event_fd != -1) {
    close(_event_fd);
    _event_fd = -1;
  }
}

void Worker::_initTimerFd() {
  _timer_fd = timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
  if (_timer_fd == -1) {
    LOG->critical("Worker {} failed to create timerfd: {}.", getWorkerId(), strerror(errno));
    exit(1);
  }
}

void Worker::_closeTimerFd() {
  if (_timer_fd != -1) {
    close(_timer_fd);
    _timer_fd = -1;
  }
}

void Worker::_signalWork() {
  if (_event_fd == -1) {
    return;
  }

  uint64_t inc = 1;
  ssize_t ret;
  do {
    ret = ::write(_event_fd, &inc, sizeof(inc));
  } while (ret == -1 && errno == EINTR);
  if (ret == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
    LOG->trace("Worker {} failed to signal eventfd: {}.", getWorkerId(), strerror(errno));
  }
}

void Worker::_drainEventFd() {
  if (_event_fd == -1) {
    return;
  }

  uint64_t value = 0;
  while (true) {
    ssize_t ret = ::read(_event_fd, &value, sizeof(value));
    if (ret > 0) {
      continue;
    }
    if (ret == -1 && errno == EINTR) {
      continue;
    }
    if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      break;
    }
    break;
  }
}

void Worker::_drainTimerFd() {
  if (_timer_fd == -1) {
    return;
  }

  uint64_t value = 0;
  while (true) {
    ssize_t ret = ::read(_timer_fd, &value, sizeof(value));
    if (ret > 0) {
      continue;
    }
    if (ret == -1 && errno == EINTR) {
      continue;
    }
    if (ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      break;
    }
    break;
  }
}

void Worker::_armTimerFd() {
  if (_timer_fd == -1) {
    return;
  }

  const auto nextFire = _ev->getNextTimerFireTime();
  struct itimerspec spec{};

  if (nextFire != std::chrono::milliseconds::zero()) {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
    auto delay = nextFire - now;
    if (delay <= std::chrono::milliseconds::zero()) {
      delay = std::chrono::milliseconds(0);
    }

    const auto delay_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(delay);
    time_t secs         = static_cast<time_t>(delay_ns.count() / 1000000000LL);
    long nsecs          = static_cast<long>(delay_ns.count() % 1000000000LL);
    if (secs == 0 && nsecs == 0) {
      nsecs = 1;
    }

    spec.it_value.tv_sec  = secs;
    spec.it_value.tv_nsec = nsecs;
  }

  if (timerfd_settime(_timer_fd, 0, &spec, nullptr) == -1) {
    LOG->trace("Worker {} failed to arm timerfd: {}.", getWorkerId(), strerror(errno));
  }
}

/**
 * Accept a new connection on the server socket.
 */
void Worker::_acceptConnection(bool ssl) {
  // The listening socket is non-blocking. Drain the accept backlog in a loop:
  // - We stop when accept() returns EAGAIN/EWOULDBLOCK (nothing left to accept).
  // - This avoids extra epoll wakeups and reduces latency under connection bursts.
  const int listenFd = ssl ? _server->getSSLServerSocket() : _server->getServerSocket();

  for (;;) {
    struct sockaddr_in csin;
    socklen_t clen = sizeof(csin);
    memset(reinterpret_cast<char*>(&csin), '\0', sizeof(csin));

    // accept4 avoids a separate fcntl() for non-blocking sockets on Linux.
    // On non-Linux platforms we fall back to accept(), and the Connection
    // constructor sets O_NONBLOCK.
#ifdef __linux__
    const int connectionFd = accept4(listenFd, (struct sockaddr*)&csin, &clen, SOCK_NONBLOCK | SOCK_CLOEXEC);
#else
    const int connectionFd = accept(listenFd, (struct sockaddr*)&csin, &clen);
#endif

    if (connectionFd == -1) {
      if (errno == EINTR) {
        // Interrupted by signal; retry accept().
        continue;
      }
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Backlog drained: no more pending connections right now.
        LOG->trace("accept() backlog drained.");
        break;
      }

      if (errno == EMFILE) {
        // Process file descriptor limit reached; stop accepting to avoid a tight loop.
        LOG->error("All connections available used. Cannot accept more connections.");
      } else {
        LOG->error("Could not accept new connection: {}.", strerror(errno));
      }

      break;
    }

    auto* target = _server->getWorker();
    SocketHandle socket(connectionFd);
    if (target == nullptr || !target->enqueueAcceptedSocket(std::move(socket), csin, ssl)) {
      LOG->debug("Rejected accepted connection while workers are stopping.");
    }
  }
}

/**
 * Add a new connection to this worker.
 * @param fd Filedescriptor of connection.
 * @param csin sockaddr_in for the connection.
 */
ConnectionPtr Worker::_addConnection(PendingConnection pending) {
  _assertOwnerThread();
  if (_next_connection_id > TOKEN_ID_MASK) {
    LOG->critical("Worker {} exhausted connection identifiers.", getWorkerId());
    return nullptr;
  }
  const ConnectionId connectionId = _next_connection_id++;
  ConnectionPtr connection;

  ConnectionCallbacks callbacks;
  callbacks.onHttpRequest = [this](Connection& connection, const http::Request& request) {
    if (connection.isShutdown()) {
      return;
    }
    http::Handler::handleRequest(HandlerContext(
                                     _config, _server->getRedis(), *_server->getKVStore(),
                                     [this]() { return _server->getAggregatedMetrics(); }, connection.getSharedPtr()),
                                 request);
  };
  callbacks.onHttpError = [this](Connection& connection, http::ParseError error) {
    if (connection.isShutdown()) {
      return;
    }
    http::Handler::handleError(HandlerContext(
                                   _config, _server->getRedis(), *_server->getKVStore(),
                                   [this]() { return _server->getAggregatedMetrics(); }, connection.getSharedPtr()),
                               error);
  };
  callbacks.onWebSocketMessage = [this](Connection& connection, websocket::FrameType frameType,
                                        const std::string& data) {
    if (connection.isShutdown()) {
      return;
    }
    websocket::Handler::handleMessage(HandlerContext(
                                          _config, _server->getRedis(), *_server->getKVStore(),
                                          [this]() { return _server->getAggregatedMetrics(); }, connection.getSharedPtr()),
                                      frameType, data);
  };
  callbacks.onWebSocketError = [this](Connection& connection, websocket::ParserError error) {
    if (connection.isShutdown()) {
      return;
    }
    websocket::Handler::handleError(HandlerContext(
                                        _config, _server->getRedis(), *_server->getKVStore(),
                                        [this]() { return _server->getAggregatedMetrics(); }, connection.getSharedPtr()),
                                    error);
  };

  auto transport = pending.ssl
                       ? makeTlsTransport(std::move(pending.socket), _server->getSSLContext())
                       : makeTcpTransport(std::move(pending.socket));
  connection     = std::make_shared<Connection>(connectionId, std::move(transport),
                                                pending.address, this, config(),
                                                std::move(callbacks));

  std::weak_ptr<Connection> weakConnection(connection);

  int ret = connection->addToEpoll((EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR),
                                   _connectionToken(connectionId));

  if (ret == -1) {
    LOG->warn("Could not add connection to epoll: {}.", strerror(errno));
    return nullptr;
  }

  _connections.emplace(connectionId, connection);

  LOG->trace("Connection {} accepted in worker {}", connection->getIP(), getWorkerId());

  // Disconnect if a successful WebSocket handshake hasn't occurred in time.
  addTimer(config().get<int>("handshake_timeout") * 1000, [weakConnection, this](TimerCtx* ctx) {
    auto connection = weakConnection.lock();

    if (connection && connection->protocol() == ConnectionProtocol::HTTP) {
      LOG->debug("Connection {} failed to handshake in {} seconds. Removing.", connection->getIP(), config().get<int>("handshake_timeout"));
      connection->close();
    }
  });

  // Send a WebSocket PING frame at the configured interval.
  addTimer(
      config().get<int>("ping_interval") * 1000, [weakConnection](TimerCtx* ctx) {
        auto connection = weakConnection.lock();

        if (!connection || connection->isShutdown()) {
          ctx->repeat = false;
          return;
        }

        if (connection->protocol() == ConnectionProtocol::WEBSOCKET) {
          websocket::Response::sendData(connection, "", websocket::FrameType::PING_FRAME);
        } else if (connection->protocol() == ConnectionProtocol::SSE) {
          sse::Response::sendPing(connection);
        }

        // TODO: Disconnect if the last PONG exceeds the allowed interval.
      },
      true);

  _metrics.current_connections_count++;
  _metrics.total_connect_count++;

  return connection;
}

/**
 * Remove a connection from this worker.
 * @param conn Connection to remove.
 */
void Worker::_removeConnection(ConnectionPtr conn) {
  _assertOwnerThread();

  conn->removeFromEpoll();
  if (_connections.erase(conn->id()) == 0) {
    return;
  }

  _metrics.current_connections_count--;
  _metrics.total_disconnect_count++;
}

void Worker::_drainPendingConnections() {
  _assertOwnerThread();
  std::deque<PendingConnection> pending;
  {
    std::lock_guard<std::mutex> lock(_pending_connections_mutex);
    pending.swap(_pending_connections);
  }
  if (stopRequested()) {
    return;
  }
  while (!pending.empty()) {
    auto connection = std::move(pending.front());
    pending.pop_front();
    _addConnection(std::move(connection));
  }
}

void Worker::_closeConnections() {
  _assertOwnerThread();
  for (auto& entry : _connections) {
    entry.second->removeFromEpoll();
    entry.second->close();
  }
  _metrics.total_disconnect_count += _connections.size();
  _metrics.current_connections_count = 0;
  _connections.clear();
}

void Worker::publish(const std::string& topicName, const std::string& data) {
  _ev->addJob([this, topicName, data]() {
    _topic_manager->publish(topicName, data);
  });
  _signalWork();
}

/**
 * Process epoll events and timers.
 */
void Worker::_workerMain() {
  struct epoll_event eventConnectionList[MAXEVENTS];
  struct epoll_event serverSocketEvent;
  struct epoll_event serverSocketEventSSL;

  _owner_thread = std::this_thread::get_id();
  LOG->debug("Worker {} started.", getWorkerId());

  // Set initial eventloop delay sample start time.
  _ev_delay_sample_start = Util::getTimeSinceEpoch();

  // Sample eventloop delay every <METRIC_DELAY_SAMPLE_RATE_MS> and store it in our metrics.
  addTimer(
      METRIC_DELAY_SAMPLE_RATE_MS, [&](TimerCtx* ctx) {
        const auto epoch = Util::getTimeSinceEpoch();
        long diff        = epoch - _ev_delay_sample_start - METRIC_DELAY_SAMPLE_RATE_MS;

        _metrics.eventloop_delay_ms = (diff < 0) ? 0 : diff;
        _ev_delay_sample_start      = Util::getTimeSinceEpoch();
      },
      true);

  if (_epoll_fd == -1) {
    LOG->critical("epoll_create1() failed in worker {}: {}.", getWorkerId(), strerror(errno));
    exit(1);
    return;
  }

  if (_event_fd != -1) {
    struct epoll_event eventfdEvent;
    eventfdEvent.events   = EPOLLIN;
    eventfdEvent.data.u64 = TOKEN_EVENT;
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _event_fd, &eventfdEvent) == -1) {
      LOG->critical("Failed to add eventfd to epoll in worker {}: {}.", getWorkerId(), strerror(errno));
      exit(1);
    }
  }

  if (_timer_fd != -1) {
    struct epoll_event timerEvent;
    timerEvent.events   = EPOLLIN;
    timerEvent.data.u64 = TOKEN_TIMER;
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _timer_fd, &timerEvent) == -1) {
      LOG->critical("Failed to add timerfd to epoll in worker {}: {}.", getWorkerId(), strerror(errno));
      exit(1);
    }
  }

  // Add server listening socket to epoll.
  serverSocketEvent.events   = EPOLLIN | EPOLLEXCLUSIVE;
  serverSocketEvent.data.u64 = TOKEN_LISTENER;

  if (!config().get<bool>("disable_unsecure_listener")) {
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _server->getServerSocket(), &serverSocketEvent) == -1) {
      LOG->critical("Failed to add serversocket to epoll in AcceptWorker {}: {}", getWorkerId(), strerror(errno));
      exit(1);
    }
  }

  // Add server listening socket to epoll.
  if (_server->isSSL()) {
    serverSocketEventSSL.events   = EPOLLIN | EPOLLEXCLUSIVE;
    serverSocketEventSSL.data.u64 = TOKEN_TLS_LISTENER;

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _server->getSSLServerSocket(), &serverSocketEventSSL) == -1) {
      LOG->critical("Failed to add SSL serversocket to epoll in AcceptWorker {}: {}", getWorkerId(), strerror(errno));
      exit(1);
    }
  }

  while (!stopRequested()) {
    int n = epoll_wait(_epoll_fd, eventConnectionList, MAXEVENTS, -1);

    for (int i = 0; i < n; i++) {
      const auto token = eventConnectionList[i].data.u64;
      if (token == TOKEN_EVENT) {
        _drainEventFd();
        _drainPendingConnections();
        continue;
      }
      if (token == TOKEN_TIMER) {
        _drainTimerFd();
        continue;
      }
      // Handle new connections.
      if (token == TOKEN_LISTENER || token == TOKEN_TLS_LISTENER) {
        if (eventConnectionList[i].events & EPOLLIN) {
          bool isSSL = token == TOKEN_TLS_LISTENER;
          _acceptConnection(isSSL);
        }

        continue;
      }

      if ((token & TOKEN_KIND_MASK) != 0) {
        continue;
      }
      const auto found = _connections.find(token & TOKEN_ID_MASK);
      if (found == _connections.end()) {
        continue;
      }
      auto connection = found->second;

      connection->handleEvents(eventConnectionList[i].events);

      // Read any final bytes before honoring a peer half-close. Fatal epoll
      // errors remain terminal, and close/removal is idempotent.
      if ((eventConnectionList[i].events & EPOLLERR) ||
          (eventConnectionList[i].events & EPOLLHUP) ||
          (eventConnectionList[i].events & EPOLLRDHUP)) {
        connection->close();
      }

      if (connection->isShutdown()) {
        _removeConnection(connection);
      }
    }

    // Process timers and jobs.
    try {
      _ev->process();
    } catch (const std::exception& error) {
      LOG->error("Worker {} callback failed: {}", getWorkerId(), error.what());
    } catch (...) {
      LOG->error("Worker {} callback failed with an unknown exception", getWorkerId());
    }
    _armTimerFd();
  }

  _accepting_commands.store(false, std::memory_order_release);
  _drainPendingConnections();
  _closeConnections();
}
} // namespace eventhub
