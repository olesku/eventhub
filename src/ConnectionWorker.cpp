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
#include "Connection.hpp"
#include "EventLoop.hpp"
#include "HTTPHandler.hpp"
#include "Server.hpp"
#include "websocket/Handler.hpp"
#include "Worker.hpp"

using namespace std;

namespace eventhub {

Worker::Worker(Server* srv) {
  _server   = srv;
  _epoll_fd = epoll_create1(0);

  _connection_list.reserve(50000);
  _connection_list.max_load_factor(0.25);
}

Worker::~Worker() {
  if (_epoll_fd != -1) {
    close(_epoll_fd);
  }
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
        LOG(INFO) << "Accept EAGAIN";
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
  auto client = make_shared<Connection>(fd, csin);

  LOG(INFO) << "Add connection fd: " << fd << " thread_id: " << threadId();

  _connection_list_mutex.lock();
  int ret = client->addToEpoll(_epoll_fd, (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR));

  if (ret == -1) {
    _connection_list_mutex.unlock();
    LOG(WARNING) << "Could not add client to epoll: " << strerror(errno);
    return;
  }

  _connection_list.emplace(make_pair(fd, client));
  _connection_list_mutex.unlock();

  LOG(INFO) << "Client " << fd << " accepted in worker " << threadId();

  std::weak_ptr<Connection> wptrConnection(client);
  _ev.addTimer(10000, [this, wptrConnection](EventLoop::TimerCtxT* ctx) {
    auto c = wptrConnection.lock();

    if (c && c->getState() != Connection::WEBSOCKET_MODE) {
      LOG(INFO) << "Client " << c->getIP() << " failed to handshake in 10 seconds. Removing.";
      c->shutdown();
    }
  });
}

void Worker::_read(std::shared_ptr<Connection>& client) {
  char rBuf[1024];
  ssize_t bytesRead = client->read(rBuf, 1024);

  if (bytesRead < 1) {
    client->shutdown();
    return;
  }

  if (client->getState() != Connection::WEBSOCKET_MODE) {
    DLOG(INFO) << "Read: " << rBuf;
  }

  // Parse request if in parse state.
  switch (client->getState()) {
    case Connection::HTTP_MODE:
      HTTPHandler::parse(client, this, rBuf, bytesRead);
      break;

    case Connection::WEBSOCKET_MODE:
      websocket::Handler::process(client, this, rBuf, bytesRead);
      break;

    default:
      DLOG(ERROR) << "Connection " << client->getIP() << " has invalid state, disconnecting.";
      client->shutdown();
  }
}

void Worker::_removeConnection(const connection_list_t::iterator& it) {
  _connection_list.erase(it);
}

void Worker::subscribeConnection(std::shared_ptr<Connection>& conn, const string& topicFilterName) {
  _topic_manager.subscribeConnection(conn, topicFilterName);
}

void Worker::publish(const string& topicName, const string& data) {
  _ev.addJob([this, topicName, data]() {
    _topic_manager.publish(topicName, data);
  });
}

void Worker::workerMain() {
  std::shared_ptr<struct epoll_event[]> eventConnectionList(new struct epoll_event[MAXEVENTS]);
  struct epoll_event serverSocketEvent;

  DLOG(INFO) << "Worker thread " << threadId() << " started.";

  if (_epoll_fd == -1) {
    LOG(FATAL) << "epoll_create1() failed in worker " << threadId() << ": " << strerror(errno);
    return;
  }

  // Add server listening socket to epoll.
  serverSocketEvent.events  = EPOLLIN | EPOLLEXCLUSIVE;
  serverSocketEvent.data.fd = _server->getServerSocket();

  LOG_IF(FATAL, epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _server->getServerSocket(), &serverSocketEvent) == -1)
      << "Failed to add serversocket to epoll in AcceptWorker " << threadId();

  // Run garbage collection of topics with no more connections.
  _ev.addTimer(
      20000, [this](EventLoop::TimerCtxT* ctx) {
        _topic_manager.garbageCollect();
      },
      true);

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
        LOG(ERROR) << "ERROR: Received event on filedescriptor which is not present in client list fd: " << eventConnectionList[i].data.fd << " thread_id: " << threadId();
        epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, eventConnectionList[i].data.fd, 0);
        close(eventConnectionList[i].data.fd);
        _connection_list_mutex.unlock();
        continue;
      }

      auto& client = clientIt->second;
      _connection_list_mutex.unlock();

      // Close socket if an error occurs.
      if (eventConnectionList[i].events & EPOLLERR) {
        DLOG(WARNING) << "Error occurred while reading data from client " << client->getIP() << ".";
        _removeConnection(clientIt);
        continue;
      }

      if ((eventConnectionList[i].events & EPOLLHUP) || (eventConnectionList[i].events & EPOLLRDHUP)) {
        DLOG(WARNING) << "Client " << client->getIP() << " disconnected.";
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

  DLOG(INFO) << "Connection worker " << threadId() << " destroyed.";
}
} // namespace eventhub
