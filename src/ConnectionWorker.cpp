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
#include "http/Handler.hpp"
#include "Server.hpp"
#include "Worker.hpp"
#include "websocket/Handler.hpp"
#include "websocket/Response.hpp"

using namespace std;

namespace eventhub {

Worker::Worker(Server* srv, unsigned int workerId) : _workerId(workerId) {
  _server   = srv;
  _epoll_fd = epoll_create1(0);

  _connection_list.reserve(50000);
  _connection_list.max_load_factor(0.25);
}

Worker::~Worker() {
  if (_epoll_fd != -1) {
    close(_epoll_fd);
  }

  DLOG(INFO) << "Connection worker " << getWorkerId() << " destroyed.";
}

void Worker::addTimer(int64_t delay, std::function<void(TimerCtx* ctx)> callback, bool repeat) {
  _ev.addTimer(delay, callback, repeat);
}

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

  // Create the client object and add it to our client list.
  _server->getWorker()->_addConnection(clientFd, &csin);
}

void Worker::_addConnection(int fd, struct sockaddr_in* csin) {
  auto client = make_shared<Connection>(fd, csin, this);

  _connection_list_mutex.lock();
  int ret = client->addToEpoll(_epoll_fd, (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR));

  if (ret == -1) {
    _connection_list_mutex.unlock();
    LOG(WARNING) << "Could not add client to epoll: " << strerror(errno);
    return;
  }

  _connection_list.emplace(make_pair(fd, client));
  _connection_list_mutex.unlock();

  //DLOG(INFO) << "Client " << fd << " accepted in worker " << getWorkerId();

  // Disconnect client if successful websocket handshake hasn't occurred in 10 seconds.
  std::weak_ptr<Connection> wptrConnection(client);
  addTimer(10000, [this, wptrConnection](TimerCtx* ctx) {
    auto c = wptrConnection.lock();

    if (c && c->getState() != ConnectionState::WEBSOCKET) {
      LOG(INFO) << "Client " << c->getIP() << " failed to handshake in 10 seconds. Removing.";
      c->shutdown();
    }
  });

  // Send a websocket PING frame to the client every Config.getPingInterval() second.
  addTimer(Config.getPingInterval() * 1000, [wptrConnection](TimerCtx* ctx) {
    auto c = wptrConnection.lock();

    if (!c || c->isShutdown()) {
      ctx->repeat = false;
      return;
    }

    if (c->getState() == ConnectionState::WEBSOCKET) {
      websocket::response::sendData(c, "", websocket::response::PING_FRAME, 1);
    }

    // TODO: Disconnect client if lastPong was Config.getPingInterval() * 1000 * 3 ago.
  }, true);
}

void Worker::_read(ConnectionPtr client) {
  char rBuf[8096];
  ssize_t bytesRead = client->read(rBuf, 8096);

  if (bytesRead < 1) {
    client->shutdown();
    return;
  }

  // Parse request if in parse state.
  switch (client->getState()) {
    case ConnectionState::HTTP:
      http::Handler::process(client, rBuf, bytesRead);
      break;

    case ConnectionState::WEBSOCKET :
      websocket::Handler::process(client, rBuf, bytesRead);
      break;

    default:
      DLOG(ERROR) << "Connection " << client->getIP() << " has invalid state, disconnecting.";
      client->shutdown();
  }
}

void Worker::_removeConnection(const connection_list_t::iterator& it) {
  _connection_list.erase(it);
}

void Worker::publish(const string& topicName, const string& data) {
  _ev.addJob([this, topicName, data]() {
    _topic_manager.publish(topicName, data);
  });
}

void Worker::_workerMain() {
  std::shared_ptr<struct epoll_event[]> eventConnectionList(new struct epoll_event[MAXEVENTS]);
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

    int n = epoll_wait(_epoll_fd, eventConnectionList.get(), MAXEVENTS, timeout);

    for (int i = 0; i < n; i++) {
      // Handle new connections.
      if (eventConnectionList[i].data.fd == _server->getServerSocket()) {
        if (eventConnectionList[i].events & EPOLLIN) {
          _acceptConnection();
        }

        continue;
      }

      _connection_list_mutex.lock();
      auto clientIt = _connection_list.find(eventConnectionList[i].data.fd);
      if (clientIt == _connection_list.end()) {
        LOG(ERROR) << "ERROR: Received event on filedescriptor which is not present in client list fd: " << eventConnectionList[i].data.fd << " worker: " << getWorkerId();
        epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, eventConnectionList[i].data.fd, 0);
        close(eventConnectionList[i].data.fd);
        _connection_list_mutex.unlock();
        continue;
      }

      auto& client = clientIt->second;
      _connection_list_mutex.unlock();

      // Close socket if an error occurs.
      if (eventConnectionList[i].events & EPOLLERR) {
        //DLOG(WARNING) << "Error occurred while reading data from client " << client->getIP() << ".";
        _removeConnection(clientIt);
        continue;
      }

      if ((eventConnectionList[i].events & EPOLLHUP) || (eventConnectionList[i].events & EPOLLRDHUP)) {
        //DLOG(WARNING) << "Client " << client->getIP() << " disconnected.";
        _removeConnection(clientIt);
        continue;
      }

      if (eventConnectionList[i].events & EPOLLOUT) {
        DLOG(INFO) << client->getIP() << ": EPOLLOUT, flushing send buffer.";
        client->flushSendBuffer();
        continue;
      }

      if (client->isShutdown()) {
        DLOG(ERROR) << "Client " << client->getIP() << " is marked as shutdown, should be handled by EPOLL(RD)HUP, removing.";
        _removeConnection(clientIt);
        continue;
      }

      _read(client);
    }

    _ev.process();
  }
}
} // namespace eventhub
