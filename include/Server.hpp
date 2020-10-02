#ifndef INCLUDE_SERVER_HPP_
#define INCLUDE_SERVER_HPP_

#include <memory>
#include <mutex>
#include <string>

#include "ConnectionWorker.hpp"
#include "Redis.hpp"
#include "Worker.hpp"
#include "metrics/Types.hpp"
#include "EventLoop.hpp"
#include "SSL.hpp"

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
  inline bool isSSL() { return _ssl_enabled; }
  inline SSL_CTX* getSSLContext() { assert(isSSL()); return _ssl_ctx.get(); }

private:
  int _server_socket;
  const SSL_METHOD *_ssl_method;
  OpenSSLUniquePtr<SSL_CTX> _ssl_ctx;
  WorkerGroup<Worker> _connection_workers;
  WorkerGroup<Worker>::iterator _cur_worker;
  std::mutex _connection_workers_lock;
  Redis _redis;
  metrics::ServerMetrics _metrics;
  EventLoop _ev;
  bool _ssl_enabled;

  void _initSSLContext();
};

} // namespace eventhub

#endif // INCLUDE_SERVER_HPP_
