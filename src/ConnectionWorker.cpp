#include <errno.h>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <netinet/in.h>
#include <stdexcept>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Common.hpp"
#include "Config.hpp"
#include "Connection.hpp"
#include "EventLoop.hpp"
#include "Server.hpp"
#include "Worker.hpp"
#include "http/Handler.hpp"
#include "websocket/Handler.hpp"
#include "websocket/Parser.hpp"
#include "websocket/Response.hpp"
#include "HandlerContext.hpp"

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

  _connection_list_mutex.lock();

  for (auto it = _connection_list.begin(); it != _connection_list.end();) {
    it = _connection_list.erase(it);
  }

  DLOG(INFO) << "Connection worker " << getWorkerId() << " destroyed.";
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

  memset((char*)&csin, '\0', sizeof(csin));
  clen = sizeof(csin);

// Accept the connection.
do_accept:
  clientFd = accept(_server->getServerSocket(), (struct sockaddr*)&csin, &clen);

  if (clientFd == -1) {
    switch (errno) {
      case EMFILE:
        LOG(ERROR) << "All connections available used. Cannot accept more connections.";
        break;

      case EAGAIN:
        //DLOG(INFO) << "Accept EAGAIN";
        //goto do_accept;
        break;

      default:
        LOG(ERROR) << "Error accepting new connection: " << strerror(errno);
    }

    return;
  }

  // Create the client object and assign it to a worker.
  _server->getWorker()->_addConnection(clientFd, &csin);
}

/**
 * Add a new connection to this worker.
 * @param fd Filedescriptor of connection.
 * @param csin sockaddr_in for the connection.
 */
void Worker::_addConnection(int fd, struct sockaddr_in* csin) {
  std::lock_guard<std::mutex> lock(_connection_list_mutex);

  auto client = make_shared<Connection>(fd, csin, this);

  auto connectionIterator = _connection_list.insert(_connection_list.end(), client);
  int ret = client->addToEpoll(connectionIterator, (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR));

  if (ret == -1) {
    LOG(WARNING) << "Could not add client to epoll: " << strerror(errno);
    _connection_list.erase(connectionIterator);
    return;
  }

  //DLOG(INFO) << "Client " << fd << " accepted in worker " << getWorkerId();
  std::weak_ptr<Connection> wptrConnection(client);

  // Set up HTTP request callback.
  client->onHTTPRequest([this, wptrConnection](http::Parser* req, http::RequestState reqState) {
    auto c = wptrConnection.lock();
    if (!c) return;
    http::Handler::HandleRequest(HandlerContext(_server, this, c), req, reqState);
  });

  // Set up websocket request callback.
  client->onWebsocketRequest([this, wptrConnection](websocket::ParserStatus status,
                                                  websocket::FrameType frameType,
                                                  const std::string& data)
  {
    auto c = wptrConnection.lock();
    if (!c) return;
    websocket::Handler::HandleRequest(HandlerContext(_server, this, c),
                                      status, frameType, data);
  });


  // Disconnect client if successful websocket handshake hasn't occurred in 10 seconds.
  addTimer(10000, [this, wptrConnection](TimerCtx* ctx) {
    auto c = wptrConnection.lock();

    if (c && c->getState() != ConnectionState::WEBSOCKET) {
      LOG(INFO) << "Client " << c->getIP() << " failed to handshake in 10 seconds. Removing.";
      c->shutdown();
    }
  });

  // Send a websocket PING frame to the client every Config.getPingInterval() second.
  addTimer(
      Config.getPingInterval() * 1000, [wptrConnection](TimerCtx* ctx) {
        auto c = wptrConnection.lock();

        if (!c || c->isShutdown()) {
          ctx->repeat = false;
          return;
        }

        if (c->getState() == ConnectionState::WEBSOCKET) {
          //DLOG(INFO) << "Ping " << c->getIP();
          websocket::response::sendData(c, "", websocket::FrameType::PING_FRAME);
        }

        // TODO: Disconnect client if lastPong was Config.getPingInterval() * 1000 * 3 ago.
      },
      true);
}

/**
 * Remove a connection from this worker.
 * @param conn Connection to remove.
 */
void Worker::_removeConnection(ConnectionPtr conn) {
  std::lock_guard<std::mutex> lock(_connection_list_mutex);
  _connection_list.erase(conn->getConnectionListIterator());
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

  LOG(INFO) << "Worker " << getWorkerId() << " started.";

  if (_epoll_fd == -1) {
    LOG(FATAL) << "epoll_create1() failed in worker " << getWorkerId() << ": " << strerror(errno);
    return;
  }

  // Add server listening socket to epoll.
  serverSocketEvent.events  = EPOLLIN | EPOLLEXCLUSIVE;
  serverSocketEvent.data.fd = _server->getServerSocket();

  LOG_IF(FATAL, epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _server->getServerSocket(), &serverSocketEvent) == -1)
      << "Failed to add serversocket to epoll in AcceptWorker " << getWorkerId();

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

      // Close socket if an error occurs.
      if (eventConnectionList[i].events & EPOLLERR) {
        //DLOG(WARNING) << "Error occurred while reading data from client " << client->getIP() << ".";
        _removeConnection(client);
        continue;
      }

      if ((eventConnectionList[i].events & EPOLLHUP) || (eventConnectionList[i].events & EPOLLRDHUP)) {
        //DLOG(WARNING) << "Client " << client->getIP() << " disconnected.";
        _removeConnection(client);
        continue;
      }

      if (eventConnectionList[i].events & EPOLLOUT) {
        DLOG(INFO) << client->getIP() << ": EPOLLOUT, flushing send buffer.";
        client->flushSendBuffer();
        continue;
      }

      if (client->isShutdown()) {
        DLOG(ERROR) << "Client " << client->getIP() << " is marked as shutdown, should be handled by EPOLL(RD)HUP, removing.";
        _removeConnection(client);
        continue;
      }

      client->read();
    }

    // Process timers and jobs.
    _ev.process();
  }
}
} // namespace eventhub
