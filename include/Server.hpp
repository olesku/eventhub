#ifndef INCLUDE_SERVER_HPP_
#define INCLUDE_SERVER_HPP_

#include <memory>
#include <mutex>
#include <string>
#include <openssl/ssl.h>

#include "ConnectionWorker.hpp"
#include "Redis.hpp"
#include "Worker.hpp"
#include "metrics/Types.hpp"
#include "EventLoop.hpp"

using namespace std;

namespace eventhub {

class Server {
public:
  Server(const string redisHost, int redisPort, const std::string redisPassword, int redisPoolSize);
  ~Server();

  void start();
  void stop();
  const int getServerSocket();
  const int getServerSSLSocket();
  Worker* getWorker();
  void publish(const std::string topicName, const std::string data);
  inline Redis& getRedis() { return _redis; }
  metrics::AggregatedMetrics getAggregatedMetrics();

private:
  int _server_socket;
  int _ssl_server_socket;
  const SSL_METHOD *_ssl_method;
  SSL_CTX* _ssl_ctx;
  WorkerGroup<Worker> _connection_workers;
  WorkerGroup<Worker>::iterator _cur_worker;
  std::mutex _connection_workers_lock;
  Redis _redis;
  metrics::ServerMetrics _metrics;
  EventLoop _ev;
};

} // namespace eventhub

#endif // INCLUDE_SERVER_HPP_
