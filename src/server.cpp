#include "server.hpp"
#include "common.hpp"
#include <fcntl.h>
#include <mutex>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>

namespace eventhub {

Server::Server() {
}

Server::~Server() {
  DLOG(INFO) << "Server destructor.";
  stop();
}

void Server::start() {
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
  sin.sin_family = AF_INET;
  sin.sin_port   = htons(8080);

  LOG_IF(FATAL, (bind(_server_socket, (struct sockaddr*)&sin, sizeof(sin))) == -1) << "Could not bind server socket to port 8080";

  LOG_IF(FATAL, listen(_server_socket, 0) == -1) << "Call to listen() failed.";
  LOG_IF(FATAL, fcntl(_server_socket, F_SETFL, O_NONBLOCK) == -1) << "fcntl O_NONBLOCK on serversocket failed.";

  LOG(INFO) << "Listening on port 8080.";

  // Start the connection workers.
  for (unsigned i = 0; i < std::thread::hardware_concurrency(); i++) {
    DLOG(INFO) << "Added worker " << i;
    _connection_workers.addWorker(new Worker(this));
  }

  _cur_worker = _connection_workers.begin();

  // Connect to redis.
  _redis_subscriber.connect("127.0.0.1", "6379");
  LOG_IF(FATAL, !_redis_subscriber.isConnected()) << "Could not connect to Redis.";

  _redis_subscriber.psubscribe("eventhub.*", [&](const std::string& pattern, const std::string& channel, const std::string& data) {
    auto ch = channel.substr(9, std::string::npos);

    DLOG(INFO) << "Publishing event to channel '" << ch << "'";
    publish(ch, data);
  });
}

Worker* Server::getWorker() {
  if (_cur_worker == _connection_workers.end()) {
    _cur_worker = _connection_workers.begin();
  }

  return (_cur_worker++)->get();
}

void Server::publish(const string topic_name, const string data) {
  std::lock_guard<std::mutex> lock(_publish_lock);
  for (auto& worker : _connection_workers.getWorkerList()) {
    worker->publish(topic_name, data);
  }
}

void Server::stop() {
  _redis_subscriber.disconnect();
  close(_server_socket);
  _connection_workers.killAndDeleteAll();
}

const int Server::get_server_socket() {
  return _server_socket;
}

} // namespace eventhub
