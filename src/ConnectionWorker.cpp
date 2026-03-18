#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <spdlog/logger.h>

#include "ConnectionWorker.hpp"
#include "Logger.hpp"
#include "http/Parser.hpp"
#include "websocket/Types.hpp"
#ifdef __linux__
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>
#else
#error "eventhub worker requires Linux (epoll/eventfd/timerfd)"
#endif
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <atomic>
#include <type_traits>

#include "Common.hpp"
#include "Config.hpp"
#include "Connection.hpp"
#include "EventLoop.hpp"
#include "HandlerContext.hpp"
#include "SSLConnection.hpp"
#include "Server.hpp"
#include "TopicManager.hpp"
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

  _ev = std::make_unique<EventLoop>();
  _topic_manager = std::make_unique<TopicManager>();

  _initEventFd();
  _initTimerFd();
}

Worker::~Worker() {
  if (_epoll_fd != -1) {
    close(_epoll_fd);
  }
  _closeEventFd();
  _closeTimerFd();

  std::lock_guard<std::mutex> lock(_connection_list_mutex);

  for (auto it = _connection_list.begin(); it != _connection_list.end();) {
    it = _connection_list.erase(it);
  }

  LOG->debug("Connection worker {} shutting down.", getWorkerId());
}

void Worker::addTimer(int64_t delay, std::function<void(TimerCtx* ctx)> callback, bool repeat) {
  _ev->addTimer(delay, callback, repeat);
  _armTimerFd();
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
  ssize_t ret = ::write(_event_fd, &inc, sizeof(inc));
  if (ret == -1 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
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
  struct itimerspec spec {};

  if (nextFire != std::chrono::milliseconds::zero()) {
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
    auto delay = nextFire - now;
    if (delay <= std::chrono::milliseconds::zero()) {
      delay = std::chrono::milliseconds(0);
    }

    const auto delay_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(delay);
    time_t secs = static_cast<time_t>(delay_ns.count() / 1000000000LL);
    long nsecs = static_cast<long>(delay_ns.count() % 1000000000LL);
    if (secs == 0 && nsecs == 0) {
      nsecs = 1;
    }

    spec.it_value.tv_sec = secs;
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
    const int clientFd = accept4(listenFd, (struct sockaddr*)&csin, &clen, SOCK_NONBLOCK);
#else
    const int clientFd = accept(listenFd, (struct sockaddr*)&csin, &clen);
#endif

    if (clientFd == -1) {
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

    _server->getWorker()->_addConnection(clientFd, &csin, ssl);
  }
}

/**
 * Add a new connection to this worker.
 * @param fd Filedescriptor of connection.
 * @param csin sockaddr_in for the connection.
 */
ConnectionPtr Worker::_addConnection(int fd, struct sockaddr_in* csin, bool ssl) {
  std::lock_guard<std::mutex> lock(_connection_list_mutex);
  ConnectionListIterator connectionIterator;

  if (ssl) {
    connectionIterator = _connection_list.insert(_connection_list.end(), std::make_shared<SSLConnection>(fd, csin, this, config(), _server->getSSLContext()));
  } else {
    connectionIterator = _connection_list.insert(_connection_list.end(), std::make_shared<Connection>(fd, csin, this, config()));
  }

  auto client = connectionIterator->get()->getSharedPtr();
  std::weak_ptr<Connection> wptrClient(client);

  // Set up HTTP request callback.
  client->onHTTPRequest([this, wptrClient](http::Parser* req, http::RequestState reqState) {
    auto c = wptrClient.lock();
    if (!c)
      return;
    http::Handler::HandleRequest(HandlerContext(_config, _server, this, c), req, reqState);
  });

  // Set up websocket request callback.
  client->onWebsocketRequest([this, wptrClient](websocket::ParserStatus status,
                                                    websocket::FrameType frameType,
                                                    const std::string& data) {
    auto c = wptrClient.lock();
    if (!c)
      return;
    websocket::Handler::HandleRequest(HandlerContext(_config, _server, this, c),
                                      status, frameType, data);
  });

  client->assignConnectionListIterator(connectionIterator);
  int ret = client->addToEpoll((EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR));

  if (ret == -1) {
    LOG->warn("Could not add client to epoll: {}.", strerror(errno));
    _connection_list.erase(connectionIterator);
    return nullptr;
  }

  LOG->trace("Client {} accepted in worker {}", client->getIP(), getWorkerId());

  // Disconnect client if successful websocket handshake hasn't occurred in 10 seconds.
  addTimer(config().get<int>("handshake_timeout") * 1000, [wptrClient, this](TimerCtx* ctx) {
    auto c = wptrClient.lock();

    if (c && c->getState() != ConnectionState::WEBSOCKET && c->getState() != ConnectionState::SSE) {
      LOG->debug("Client {} failed to handshake in {} seconds. Removing.", c->getIP(), config().get<int>("handshake_timeout"));
      c->shutdown();
    }
  });

  // Send a websocket PING frame to the client every Config.getPingInterval() second.
  addTimer(
      config().get<int>("ping_interval") * 1000, [wptrClient](TimerCtx* ctx) {
        auto c = wptrClient.lock();

        if (!c || c->isShutdown()) {
          ctx->repeat = false;
          return;
        }

        if (c->getState() == ConnectionState::WEBSOCKET) {
          websocket::Response::sendData(c, "", websocket::FrameType::PING_FRAME);
        } else if (c->getState() == ConnectionState::SSE) {
          sse::Response::sendPing(c);
        }

        // TODO: Disconnect client if lastPong was Config.getPingInterval() * 1000 * 3 ago.
      },
      true);

  _metrics.current_connections_count++;
  _metrics.total_connect_count++;

  return client;
}

/**
 * Remove a connection from this worker.
 * @param conn Connection to remove.
 */
void Worker::_removeConnection(ConnectionPtr conn) {
  std::lock_guard<std::mutex> lock(_connection_list_mutex);

  conn->removeFromEpoll();
  _connection_list.erase(conn->getConnectionListIterator());

  _metrics.current_connections_count--;
  _metrics.total_disconnect_count++;
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

  LOG->debug("Worker {} started.", getWorkerId());

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
    eventfdEvent.events = EPOLLIN;
    eventfdEvent.data.fd = _event_fd;
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _event_fd, &eventfdEvent) == -1) {
      LOG->critical("Failed to add eventfd to epoll in worker {}: {}.", getWorkerId(), strerror(errno));
      exit(1);
    }
  }

  if (_timer_fd != -1) {
    struct epoll_event timerEvent;
    timerEvent.events = EPOLLIN;
    timerEvent.data.fd = _timer_fd;
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _timer_fd, &timerEvent) == -1) {
      LOG->critical("Failed to add timerfd to epoll in worker {}: {}.", getWorkerId(), strerror(errno));
      exit(1);
    }
  }

  // Add server listening socket to epoll.
  serverSocketEvent.events  = EPOLLIN | EPOLLEXCLUSIVE;
  serverSocketEvent.data.fd = _server->getServerSocket();

  if (!config().get<bool>("disable_unsecure_listener")) {
    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _server->getServerSocket(), &serverSocketEvent) == -1) {
      LOG->critical("Failed to add serversocket to epoll in AcceptWorker {}: {}", getWorkerId(), strerror(errno));
      exit(1);
    }
  }

  // Add server listening socket to epoll.
  if (_server->isSSL()) {
    serverSocketEventSSL.events  = EPOLLIN | EPOLLEXCLUSIVE;
    serverSocketEventSSL.data.fd = _server->getSSLServerSocket();

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _server->getSSLServerSocket(), &serverSocketEventSSL) == -1) {
      LOG->critical("Failed to add SSL serversocket to epoll in AcceptWorker {}: {}", getWorkerId(), strerror(errno));
      exit(1);
    }
  }

  while (!stopRequested()) {
    int n = epoll_wait(_epoll_fd, eventConnectionList, MAXEVENTS, -1);

    for (int i = 0; i < n; i++) {
      if (_event_fd != -1 && eventConnectionList[i].data.fd == _event_fd) {
        _drainEventFd();
        continue;
      }
      if (_timer_fd != -1 && eventConnectionList[i].data.fd == _timer_fd) {
        _drainTimerFd();
        continue;
      }
      // Handle new connections.
      if (eventConnectionList[i].data.fd == _server->getServerSocket() || eventConnectionList[i].data.fd == _server->getSSLServerSocket()) {
        if (eventConnectionList[i].events & EPOLLIN) {
          bool isSSL = eventConnectionList[i].data.fd == _server->getSSLServerSocket();
          _acceptConnection(isSSL);
        }

        continue;
      }

      auto client = static_cast<Connection*>(eventConnectionList[i].data.ptr)->getSharedPtr();

      // Mark the client for shutdown if client disconnects or
      // if there is an error.
      if ((eventConnectionList[i].events & EPOLLERR) || (eventConnectionList[i].events & EPOLLHUP) || (eventConnectionList[i].events & EPOLLRDHUP)) {
        client->shutdown();
      }

      // If client is marked for shutdown remove the connection.
      if (client->isShutdown()) {
        _removeConnection(client);
        continue;
      }

      // Flush send buffer if socket is ready for write.
      if (eventConnectionList[i].events & EPOLLOUT) {
        client->flushSendBuffer();
        continue;
      }

      // Read data from client if data is available.
      if (eventConnectionList[i].events & EPOLLIN) {
        client->read();
      }
    }

    // Process timers and jobs.
    _ev->process();
    _armTimerFd();
  }
}
} // namespace eventhub
