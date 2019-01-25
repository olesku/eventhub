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
    _new_connection(client_fd, &csin);

    DLOG(INFO) << "Client accepted in worker " << thread_id();
  }

  void connection_worker::_new_connection(int fd, struct sockaddr_in* csin) {
    std::lock_guard<std::mutex> guard(_connection_list_mutex);
    auto client = make_shared<connection>(fd, csin);

    int ret = client->add_to_epoll(_epoll_fd, (EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR));
    if (ret == -1) {
      LOG(WARNING) << "Could not add client to epoll: " << strerror(errno);
      return;
    }
  
    _connection_list.emplace(make_pair(fd, client));
  }

  void connection_worker::_read(std::shared_ptr<connection> client, eventhub::connection_list::iterator it) {

    char r_buf[1024];
    client->read(r_buf, 1024);

    DLOG(INFO) << "Read: " << r_buf;

    switch(client->get_state()) {
      case connection::state::INIT:

      break;

      case connection::state::HTTP_PARSE:

      break;
    }
  }

/*
  void connection_worker::_read(std::shared_ptr<connection> client, eventhub::connection_list::iterator it) {
    char buf[1024];

    // Read from client.
    size_t len = client->read(&buf, 1);

    if (len <= 0) {
      _connection_list.erase(it);
    }

    buf[len] = '\0';

    LOG(INFO) << "Read: " << buf;

    // Parse the request.
    HTTPRequest* req = client->GetHttpReq();
    HttpReqStatus reqRet = req->Parse(buf, 1024);

    switch(reqRet) {
      case HTTP_REQ_INCOMPLETE: return;

      case HTTP_REQ_FAILED:
        return;

      case HTTP_REQ_TO_BIG:
        return;

      case HTTP_REQ_OK: break;

      case HTTP_REQ_POST_INVALID_LENGTH:
        { HTTPResponse res(411, "", false); client->write(res.Get()); }
        return;

      case HTTP_REQ_POST_TOO_LARGE:
        DLOG(INFO) << "Client " <<  client->get_ip() << " sent too much POST data.";
        { HTTPResponse res(413, "", false); client->write(res.Get()); }
        return;

      case HTTP_REQ_POST_START:
        return;

      case HTTP_REQ_POST_INCOMPLETE: return;

      case HTTP_REQ_POST_OK:
        return;
    }

    HTTPResponse res;
    res.SetStatus(200);

    string s;
    for (int i = 0; i < 1000000; i++) {
      s.append(".");
    }

    res.SetBody(s + "\nYour request was: " + req->GetMethod() + " " + req->GetPath() + ". Num clients: " + std::to_string(_connection_list.size()) + "\n");
    client->write(res.Get());
    //shutdown(client->Getfd(), SHUT_RDWR);
  }
*/

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
      int n = epoll_wait(_epoll_fd, event_list.get(), MAXEVENTS, 100); // 100ms timeout.

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

        _read(client, client_it);
      }
    }

    DLOG(INFO) << "Connection worker " << thread_id() << " destroyed.";
  }
}
