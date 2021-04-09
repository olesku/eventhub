#ifndef INCLUDE_SERVER_HPP_
#define INCLUDE_SERVER_HPP_

#include <memory>
#include <mutex>
#include <openssl/ssl.h>
#include <string>

#include "ConnectionWorker.hpp"
#include "EventLoop.hpp"
#include "Redis.hpp"
#include "Worker.hpp"
#include "metrics/Types.hpp"

using namespace std;

namespace eventhub {

class Server {
public:
  Server(const string redisHost, int redisPort, const std::string redisPassword, int redisPoolSize);
  ~Server();

  void start();
  void stop();
  void reload();
  const int getServerSocket();
  Worker* getWorker();
  void publish(const std::string topicName, const std::string data);
  inline Redis& getRedis() { return _redis; }
  metrics::AggregatedMetrics getAggregatedMetrics();
  inline bool isSSL() { return _ssl_enabled; }
  inline SSL_CTX* getSSLContext() {
    assert(isSSL());
    assert(_ssl_ctx != nullptr);
    return _ssl_ctx;
  }

private:
  int _server_socket;
  bool _ssl_enabled;
  SSL_CTX* _ssl_ctx;
  WorkerGroup<Worker> _connection_workers;
  WorkerGroup<Worker>::iterator _cur_worker;
  std::mutex _connection_workers_lock;
  Redis _redis;
  metrics::ServerMetrics _metrics;
  EventLoop _ev;

  void _initSSLContext();
  void _loadSSLCertificates();
};

} // namespace eventhub

#endif // INCLUDE_SERVER_HPP_
