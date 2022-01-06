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

namespace eventhub {

class Server final {
public:
  Server(Config& cfg);
  ~Server();

  void start();
  void stop();
  void reload();
  Config& config() { return _config; }
  int getServerSocket() { return _server_socket; };
  Worker* getWorker();
  void publish(const std::string topicName, const std::string data);
  Redis& getRedis() { return _redis; }
  metrics::AggregatedMetrics getAggregatedMetrics();

  int getSSLServerSocket() { return _server_socket_ssl; };
  bool isSSL() { return _ssl_enabled; }
  SSL_CTX* getSSLContext() {
    assert(isSSL());
    assert(_ssl_ctx != nullptr);
    return _ssl_ctx;
  }

private:
  Config& _config;
  int _server_socket;
  int _server_socket_ssl;
  bool _ssl_enabled;
  SSL_CTX* _ssl_ctx;
  std::string _ssl_cert_md5_hash;
  std:: string _ssl_priv_key_md5_hash;
  WorkerGroup<Worker> _connection_workers;
  WorkerGroup<Worker>::iterator _cur_worker;
  std::mutex _connection_workers_lock;
  Redis _redis;
  metrics::ServerMetrics _metrics;
  EventLoop _ev;

  void _listenerInit();

  void _sslListenerInit();
  void _initSSL();
  void _loadSSLCertificates();
  void _checkSSLCertUpdated();
};

} // namespace eventhub

#endif // INCLUDE_SERVER_HPP_
