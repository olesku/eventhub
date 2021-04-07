#include "ConnectionWorker.hpp"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <string.h>
#ifdef __linux__
# include <sys/epoll.h>
#else
# include "EpollWrapper.hpp"
#endif
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <chrono>

#include "Common.hpp"
#include "Config.hpp"
#include "Connection.hpp"
#include "SSLConnection.hpp"
#include "EventLoop.hpp"
#include "HandlerContext.hpp"
#include "Server.hpp"
#include "Worker.hpp"
#include "http/Handler.hpp"
#include "websocket/Handler.hpp"
#include "websocket/Parser.hpp"
#include "websocket/Response.hpp"
#include "sse/Response.hpp"
#include "Util.hpp"

using namespace std;

namespace eventhub {

Worker::Worker(Server* srv, unsigned int workerId) : _workerId(workerId) {
  _server   = srv;
  _epoll_fd = epoll_create1(0);
}

Worker::~Worker() {
  if (_epoll_fd != -1) {
    close(_epoll_fd);
  }

  std::lock_guard<std::mutex> lock(_connection_list_mutex);

  for (auto it = _connection_list.begin(); it != _connection_list.end();) {
    it = _connection_list.erase(it);
  }

  LOG->debug("Connection worker {} shutting down.", getWorkerId());
}

void Worker::addTimer(int64_t delay, std::function<void(TimerCtx* ctx)> callback, bool repeat) {
  _ev.addTimer(delay, callback, repeat);
}

/**
 * Accept a new connection on the server socket.
 */
void Worker::_acceptConnection() {
  struct sockaddr_in csin;
  socklen_t clen;
  int clientFd;

  // Accept the connection.
  memset(reinterpret_cast<char*>(&csin), '\0', sizeof(csin));
  clen = sizeof(csin);
  clientFd = accept(_server->getServerSocket(), (struct sockaddr*)&csin, &clen);

  if (clientFd == -1) {
    switch (errno) {
      case EMFILE:
        LOG->error("All connections available used. Cannot accept more connections.");
        break;

      case EAGAIN:
        LOG->trace("accept() returned EAGAIN.");
        break;

      default:
        LOG->error("Could not accept new connection: {}.", strerror(errno));
    }

    return;
  }

  _server->getWorker()->_addConnection(clientFd, &csin);
}

/**
 * Add a new connection to this worker.
 * @param fd Filedescriptor of connection.
 * @param csin sockaddr_in for the connection.
 */
ConnectionPtr Worker::_addConnection(int fd, struct sockaddr_in* csin) {
  std::lock_guard<std::mutex> lock(_connection_list_mutex);

  auto connectionIterator = _connection_list.insert(_connection_list.end(),
    _server->isSSL() ? make_shared<SSLConnection>(fd, csin, _server, this) :
                       make_shared<Connection>(fd, csin, _server, this));

  auto client = connectionIterator->get()->getSharedPtr();

  client->assignConnectionListIterator(connectionIterator);
  int ret = client->addToEpoll((EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR));

  if (ret == -1) {
    LOG->warn("Could not add client to epoll: {}.", strerror(errno));
    _connection_list.erase(connectionIterator);
    return nullptr;
  }

  LOG->trace("Client {} accepted in worker {}", client->getIP(), getWorkerId());
  std::weak_ptr<Connection> wptrConnection(client);

  // Set up HTTP request callback.
  client->onHTTPRequest([this, wptrConnection](http::Parser* req, http::RequestState reqState) {
    auto c = wptrConnection.lock();
    if (!c)
      return;
    http::Handler::HandleRequest(HandlerContext(_server, this, c), req, reqState);
  });

  // Set up websocket request callback.
  client->onWebsocketRequest([this, wptrConnection](websocket::ParserStatus status,
                                                    websocket::FrameType frameType,
                                                    const std::string& data) {
    auto c = wptrConnection.lock();
    if (!c)
      return;
    websocket::Handler::HandleRequest(HandlerContext(_server, this, c),
                                      status, frameType, data);
  });

  // Disconnect client if successful websocket handshake hasn't occurred in 10 seconds.
  addTimer(Config.getInt("HANDSHAKE_TIMEOUT") * 1000, [wptrConnection](TimerCtx* ctx) {
    auto c = wptrConnection.lock();

    if (c && c->getState() != ConnectionState::WEBSOCKET && c->getState() != ConnectionState::SSE) {
      LOG->debug("Client {} failed to handshake in {} seconds. Removing.", c->getIP(), Config.getInt("HANDSHAKE_TIMEOUT"));
      c->shutdown();
    }
  });

  // Send a websocket PING frame to the client every Config.getPingInterval() second.
  addTimer(
      Config.getInt("PING_INTERVAL") * 1000, [wptrConnection](TimerCtx* ctx) {
        auto c = wptrConnection.lock();

        if (!c || c->isShutdown()) {
          ctx->repeat = false;
          return;
        }

        if (c->getState() == ConnectionState::WEBSOCKET) {
          websocket::response::sendData(c, "", websocket::FrameType::PING_FRAME);
        } else if (c->getState() == ConnectionState::SSE) {
          sse::response::sendPing(c);
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

void Worker::publish(const string& topicName, const string& data) {
  _ev.addJob([this, topicName, data]() {
    _topic_manager.publish(topicName, data);
  });
}

/**
 * Process epoll events and timers.
 */
void Worker::_workerMain() {
  struct epoll_event eventConnectionList[MAXEVENTS];
  struct epoll_event serverSocketEvent;

  LOG->debug("Worker {} started.", getWorkerId());

  // Set initial eventloop delay sample start time.
  _ev_delay_sample_start = Util::getTimeSinceEpoch();

  // Sample eventloop delay every <METRIC_DELAY_SAMPLE_RATE_MS> and store it in our metrics.
  _ev.addTimer(METRIC_DELAY_SAMPLE_RATE_MS, [&](TimerCtx *ctx) {
    const auto epoch = Util::getTimeSinceEpoch();
    long diff = epoch - _ev_delay_sample_start - METRIC_DELAY_SAMPLE_RATE_MS;

    _metrics.eventloop_delay_ms = (diff < 0) ? 0 : diff;
    _ev_delay_sample_start = Util::getTimeSinceEpoch();
  }, true);

  if (_epoll_fd == -1) {
    LOG->critical("epoll_create1() failed in worker {}: {}.", getWorkerId(), strerror(errno));
    exit(1);
    return;
  }

  // Add server listening socket to epoll.
  serverSocketEvent.events  = EPOLLIN | EPOLLEXCLUSIVE;
  serverSocketEvent.data.fd = _server->getServerSocket();

  if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _server->getServerSocket(), &serverSocketEvent) == -1) {
    LOG->critical("Failed to add serversocket to epoll in AcceptWorker {}: {}", getWorkerId(), strerror(errno));
    exit(1);
  }

  while (!stopRequested()) {
    unsigned int timeout = EPOLL_MAX_TIMEOUT;

    if (_ev.hasWork() && _ev.getNextTimerDelay().count() < EPOLL_MAX_TIMEOUT) {
      timeout = _ev.getNextTimerDelay().count();
    }

    int n = epoll_wait(_epoll_fd, eventConnectionList, MAXEVENTS, timeout);

    for (int i = 0; i < n; i++) {
      // Handle new connections.
      if (eventConnectionList[i].data.fd == _server->getServerSocket()) {
        if (eventConnectionList[i].events & EPOLLIN) {
          _acceptConnection();
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
        LOG->trace("EPOLLOUT for client {}", client->getIP());
        client->flushSendBuffer();
        continue;
      }

      // Read data from client if data is available.
      if (eventConnectionList[i].events & EPOLLIN) {
        client->read();
      }
    }

    // Process timers and jobs.
    _ev.process();
  }
}
} // namespace eventhub
