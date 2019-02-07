#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "common.hpp"
#include "server.hpp"

namespace eventhub {

  server::server() {

  }

  server::~server() {
    DLOG(INFO) << "Server destructor.";
    stop();
  }

  void server::start() {
    // Ignore SIGPIPE.
    signal(SIGPIPE, SIG_IGN);

    // Set up listening socket.
    _server_socket = socket(AF_INET, SOCK_STREAM, 0);
    LOG_IF(FATAL, _server_socket == -1) << "Error creating listening socket.";

    // Reuse port and address.
    int on = 1;
    setsockopt(_server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

    // Bind socket.
    struct sockaddr_in sin;
    memset((char*)&sin, '\0', sizeof(sin));
    sin.sin_family  = AF_INET;
    sin.sin_port  = htons(8080);

    LOG_IF(FATAL, (bind(_server_socket, (struct sockaddr*)&sin, sizeof(sin))) == -1) <<
      "Could not bind server socket to port 8080";

    LOG_IF(FATAL, (listen(_server_socket, 0)) == -1) << "Call to listen() failed.";
    LOG_IF(FATAL, fcntl(_server_socket, F_SETFL, O_NONBLOCK) == -1) << "fcntl O_NONBLOCK on serversocket failed.";

    LOG(INFO) << "Listening on port 8080.";

    // Start the connection workers.
    for (unsigned i = 0; i < std::thread::hardware_concurrency(); i++) {
      DLOG(INFO) << "Added worker " << i;
      _connection_workers.add_worker(new connection_worker(shared_from_this()));
    }

    _cur_worker = _connection_workers.begin();
  }

  std::shared_ptr<connection_worker>& server::get_worker() {
    if (_cur_worker == _connection_workers.end()) {
      _cur_worker = _connection_workers.begin();
    }
    
    return *(_cur_worker++);
  }

  void server::stop() {
    close(_server_socket);
    _connection_workers.kill_and_delete_all();
  }
  
  const int server::get_server_socket() {
    return _server_socket;
  }

}
