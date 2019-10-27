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

#include "common.hpp"
#include "connection.hpp"
#include "event_loop.hpp"
#include "http_handler.hpp"
#include "server.hpp"
#include "websocket/Handler.hpp"
#include "worker.hpp"

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
  int client_fd;

  memset((char*)&csin, '\0', sizeof(csin));
  clen = sizeof(csin);

// Accept the connection.
do_accept:
  client_fd = accept(_server->get_server_socket(), (struct sockaddr*)&csin, &clen);

  if (client_fd == -1) {
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
  _server->getWorker()->_addConnection(client_fd, &csin);
}

void Worker::_addConnection(int fd, struct sockaddr_in* csin) {
  auto client = make_shared<Connection>(fd, csin);

  LOG(INFO) << "Add connection fd: " << fd << " thread_id: " << thread_id();

  _connection_list_mutex.lock();
  int ret = client->addToEpoll(_epoll_fd, (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR));

  if (ret == -1) {
    _connection_list_mutex.unlock();
    LOG(WARNING) << "Could not add client to epoll: " << strerror(errno);
    return;
  }

  _connection_list.emplace(make_pair(fd, client));
  _connection_list_mutex.unlock();

  LOG(INFO) << "Client " << fd << " accepted in worker " << thread_id();

  std::weak_ptr<Connection> wptr_connection(client);
  _ev.addTimer(10000, [this, wptr_connection](EventLoop::timer_ctx_t* ctx) {
    auto c = wptr_connection.lock();

    if (c && c->get_state() != Connection::WEBSOCKET_MODE) {
      LOG(INFO) << "Client " << c->getIP() << " failed to handshake in 10 seconds. Removing.";
      c->shutdown();
    }
  });
}

void Worker::_read(std::shared_ptr<Connection>& client) {
  char r_buf[1024];
  ssize_t bytes_read = client->read(r_buf, 1024);

  if (bytes_read < 1) {
    client->shutdown();
    return;
  }

  if (client->get_state() != Connection::WEBSOCKET_MODE) {
    DLOG(INFO) << "Read: " << r_buf;
  }

  // Parse request if in parse state.
  switch (client->get_state()) {
    case Connection::HTTP_MODE:
      HTTPHandler::parse(client, this, r_buf, bytes_read);
      break;

    case Connection::WEBSOCKET_MODE:
      websocket::Handler::process(client, this, r_buf, bytes_read);
      break;

    default:
      DLOG(ERROR) << "Connection " << client->getIP() << " has invalid state, disconnecting.";
      client->shutdown();
  }
}

void Worker::_removeConnection(const connection_list_t::iterator& it) {
  _connection_list.erase(it);
}

void Worker::subscribeConnection(std::shared_ptr<Connection>& conn, const string& topic_filter_name) {
  _topic_manager.subscribeConnection(conn, topic_filter_name);
}

void Worker::publish(const string& topic_name, const string& data) {
  _ev.addJob([this, topic_name, data]() {
    _topic_manager.publish(topic_name, data);
  });
}

void Worker::workerMain() {
  std::shared_ptr<struct epoll_event[]> event_connection_list(new struct epoll_event[MAXEVENTS]);
  struct epoll_event server_socket_event;

  DLOG(INFO) << "Worker thread " << thread_id() << " started.";

  if (_epoll_fd == -1) {
    LOG(FATAL) << "epoll_create1() failed in worker " << thread_id() << ": " << strerror(errno);
    return;
  }

  // Add server listening socket to epoll.
  server_socket_event.events  = EPOLLIN | EPOLLEXCLUSIVE;
  server_socket_event.data.fd = _server->get_server_socket();

  LOG_IF(FATAL, epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _server->get_server_socket(), &server_socket_event) == -1)
      << "Failed to add serversocket to epoll in AcceptWorker " << thread_id();

  // Run garbage collection of topics with no more connections.
  _ev.addTimer(
      20000, [this](EventLoop::timer_ctx_t* ctx) {
        _topic_manager.garbageCollect();
      },
      true);

  while (!stop_requested()) {
    unsigned int timeout = EPOLL_MAX_TIMEOUT;

    if (_ev.hasWork() && _ev.getNextTimerDelay().count() < EPOLL_MAX_TIMEOUT) {
      timeout = _ev.getNextTimerDelay().count();
    }

    int n = epoll_wait(_epoll_fd, event_connection_list.get(), MAXEVENTS, timeout);

    for (int i = 0; i < n; i++) {
      // Handle new connections.
      if (event_connection_list[i].data.fd == _server->get_server_socket()) {
        if (event_connection_list[i].events & EPOLLIN) {
          _acceptConnection();
        }

        continue;
      }

      _connection_list_mutex.lock();
      auto client_it = _connection_list.find(event_connection_list[i].data.fd);
      if (client_it == _connection_list.end()) {
        LOG(ERROR) << "ERROR: Received event on filedescriptor which is not present in client list fd: " << event_connection_list[i].data.fd << " thread_id: " << thread_id();
        epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, event_connection_list[i].data.fd, 0);
        close(event_connection_list[i].data.fd);
        _connection_list_mutex.unlock();
        continue;
      }

      auto& client = client_it->second;
      _connection_list_mutex.unlock();

      // Close socket if an error occurs.
      if (event_connection_list[i].events & EPOLLERR) {
        DLOG(WARNING) << "Error occurred while reading data from client " << client->getIP() << ".";
        _removeConnection(client_it);
        continue;
      }

      if ((event_connection_list[i].events & EPOLLHUP) || (event_connection_list[i].events & EPOLLRDHUP)) {
        DLOG(WARNING) << "Client " << client->getIP() << " disconnected.";
        _removeConnection(client_it);
        continue;
      }

      if (event_connection_list[i].events & EPOLLOUT) {
        DLOG(INFO) << client->getIP() << ": EPOLLOUT, flushing send buffer.";
        client->flushSendBuffer();
        continue;
      }

      if (client->is_shutdown()) {
        DLOG(ERROR) << "Client " << client->getIP() << " is marked as shutdown, should be handled by EPOLL(RD)HUP, removing.";
        _removeConnection(client_it);
        continue;
      }

      _read(client);
    }

    _ev.process();
  }

  DLOG(INFO) << "Connection worker " << thread_id() << " destroyed.";
}
} // namespace eventhub
