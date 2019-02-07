#include <memory>
#include <mutex>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "common.hpp"
#include "server.hpp"
#include "connection.hpp"
#include "connection_worker.hpp"
#include "event_loop.hpp"

using namespace std;

namespace eventhub {
  connection_worker::connection_worker(std::shared_ptr<eventhub::server> server) {
    _server = server;
    _epoll_fd = epoll_create1(0);

    _connection_list.reserve(50000);
    _connection_list.max_load_factor(0.25);
  }

  connection_worker::~connection_worker() {
    if (_epoll_fd != -1) {
      close(_epoll_fd);
    }
  }

  void connection_worker::_accept_connection() {
    struct sockaddr_in csin;
      socklen_t clen;
      int client_fd;

      memset((char*)&csin, '\0', sizeof(csin));
      clen = sizeof(csin);

      // Accept the connection.
      client_fd = accept(_server->get_server_socket(), (struct sockaddr*)&csin, &clen);

      if (client_fd == -1) {
        switch (errno) {
          case EMFILE:
            LOG(ERROR) << "All connections available used. Cannot accept more connections.";
          break;

          default:
            LOG(ERROR) << "Error in accepting new client: " << strerror(errno);
        }

        return;
      }

    // Create the client object and add it to our client list.
    _server->get_worker()->new_connection(client_fd, &csin);
  }

  void connection_worker::new_connection(int fd, struct sockaddr_in* csin) {
    std::lock_guard<std::mutex> guard(_connection_list_mutex);
    auto client = make_shared<connection>(fd, csin);

    int ret = client->add_to_epoll(_epoll_fd, (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR));
    if (ret == -1) {
      LOG(WARNING) << "Could not add client to epoll: " << strerror(errno);
      return;
    }
  
    _connection_list.emplace(make_pair(fd, client));
     DLOG(INFO) << "Client accepted in worker " << thread_id();

    std::weak_ptr<connection> wptr_connection(_connection_list[fd]);
    _ev.add_timer(30000, [this, wptr_connection](event_loop::timer_ctx_t* ctx) {
      auto c = wptr_connection.lock();

      if (c && c->get_state() != connection::state::WEBSOCKET_MODE) {
        DLOG(INFO) << "Client " << c->get_ip() << " failed to handshake in 30 seconds. Removing.";
        _remove_connection(c);
      }
    });
  }

  void connection_worker::_remove_connection(std::shared_ptr<connection> conn) {
    shutdown(conn->get_fd(), SHUT_RDWR);
    // epoll handler should now delete the connection from _connection_list.
  }

  void connection_worker::_parse_http(std::shared_ptr<connection> client, const char* buf, ssize_t bytes_read) {
    auto& req = client->get_http_request();
    auto& ws_req = client->get_ws_request();

    switch(req.parse(buf, bytes_read)) {
      case http_request::HTTP_REQ_INCOMPLETE:
        return;
      break;

      case http_request::HTTP_REQ_OK:
        client->write(ws_req.make_handshake_response(req)->Get());

        if (ws_req.get_state() == websocket::request::state::WS_BAD_HANDSHAKE) {
          _remove_connection(client);
        } else {
          client->set_state(connection::state::WEBSOCKET_MODE);
        }
      break;

      default:
        _remove_connection(client);
    }
  }

  void connection_worker::_parse_websocket(std::shared_ptr<connection> client, char* buf, ssize_t bytes_read) {
    auto& ws_req = client->get_ws_request();
    ws_req.parse(buf, bytes_read);

    switch(ws_req.get_state()) {
      case websocket::request::state::WS_CONTROL_READY:
        LOG(INFO) << "Control Type: " << ws_req.get_control_frame_type() << " payload: " << ws_req.get_control_payload();
        ws_req.clear_control_payload();
      break;

      case websocket::request::state::WS_DATA_READY:
        LOG(INFO) << "Data: " << ws_req.get_payload();
        ws_req.clear_payload();
      break;
    }

    ws_req.set_state(websocket::request::state::WS_PARSE);
  }

  void connection_worker::_read(std::shared_ptr<connection> client) {
    char r_buf[1024];
    ssize_t bytes_read = client->read(r_buf, 1024);

    if (bytes_read < 1) {
      _remove_connection(client);
      return;
    }

    if (client->get_state() != connection::state::WEBSOCKET_MODE) {
      DLOG(INFO) << "Read: " << r_buf;
    }
    
    std::weak_ptr<connection> wptr_client(client);

    // Parse request if in parse state.
    switch(client->get_state()) {
      case connection::state::HTTP_MODE:
       _parse_http(client, r_buf, bytes_read);
      break;

      case connection::state::WEBSOCKET_MODE:
        _parse_websocket(client, r_buf, bytes_read);
      break;
    }
  }

  void connection_worker::worker_main() {
    std::shared_ptr<struct epoll_event[]> event_list(new struct epoll_event[MAXEVENTS]);
    struct epoll_event server_socket_event;

    DLOG(INFO) << "Worker thread " << thread_id() << " started.";

    if (_epoll_fd == -1) {
      LOG(FATAL) << "epoll_create1() failed in worker " << thread_id() << ": " << strerror(errno);
      return;
    }

    // Add server listening socket to epoll.
    server_socket_event.events = EPOLLIN | EPOLLEXCLUSIVE;
    server_socket_event.data.fd = _server->get_server_socket();
    LOG_IF(FATAL, epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, _server->get_server_socket(), &server_socket_event) == -1) << "Failed to add serversocket to epoll in AcceptWorker " << thread_id();

    while(!stop_requested()) {
      unsigned int timeout = EPOLL_MAX_TIMEOUT;
      
      if (_ev.has_work() && _ev.get_next_timer_delay().count() < EPOLL_MAX_TIMEOUT) {
        timeout =_ev.get_next_timer_delay().count();
      }

      int n = epoll_wait(_epoll_fd, event_list.get(), MAXEVENTS, timeout);

      for (int i = 0; i < n; i++) {
        // Handle new connections.
        if (event_list[i].data.fd == _server->get_server_socket()) {
          LOG(INFO) << "Event on server socket.";
          if (event_list[i].events & EPOLLIN) {
            _accept_connection();
          }

          continue;
        }

        auto client_it = _connection_list.find(event_list[i].data.fd);
        if (client_it == _connection_list.end()) {
          LOG(ERROR) << "ERROR: Received event on filedescriptor which is not present in client list.";
          epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, event_list[i].data.fd, 0);
          close(event_list[i].data.fd);
          continue;
        }

        shared_ptr<connection> client = client_it->second;

        // Close socket if an error occurs.
        if (event_list[i].events & EPOLLERR) {
          DLOG(WARNING) << "Error occurred while reading data from client " << client->get_ip() << ".";
          _connection_list.erase(client_it);
          continue;
        }

        if ((event_list[i].events & EPOLLHUP) || (event_list[i].events & EPOLLRDHUP)) {
          DLOG(WARNING) << "Client " << client->get_ip() << " hung up.";
          _connection_list.erase(client_it);
          continue;
        }

        if (event_list[i].events & EPOLLOUT) {
          DLOG(INFO) << client->get_ip() << ": EPOLLOUT, flushing send buffer.";
          client->flush_send_buffer();
          continue;
        }

        _read(client);
      }

      _ev.process();
    }

    DLOG(INFO) << "Connection worker " << thread_id() << " destroyed.";
  }
}
