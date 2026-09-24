#pragma once

#include <assert.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <openssl/ossl_typ.h>
#include <openssl/ssl.h>
#include <string>

#include "EventLoop.hpp"
#include "Forward.hpp"
#include "KVStore.hpp"
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
  void publish(const std::string& topicName, const std::string& data);
  Redis& getRedis() { return _redis; }
  KVStore* getKVStore() { return _kv_store.get(); }
  metrics::AggregatedMetrics getAggregatedMetrics();

  int getSSLServerSocket() { return _server_socket_ssl; };
  bool isSSL() { return _ssl_enabled; }
  SSL_CTX* getSSLContext() {
    assert(isSSL());
    assert(_ssl_ctx != nullptr);
    return _ssl_ctx.get();
  }

private:
  using SSL_CTX_ptr = std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)>;

  Config& _config;
  int _server_socket;
  int _server_socket_ssl;
  bool _ssl_enabled;
  SSL_CTX_ptr _ssl_ctx;
  std::string _ssl_cert_md5_hash;
  std::string _ssl_priv_key_md5_hash;
  WorkerGroup<Worker> _connection_workers;
  WorkerGroup<Worker>::iterator _cur_worker;
  std::mutex _connection_workers_lock;
  Redis _redis;
  std::unique_ptr<KVStore> _kv_store;
  metrics::ServerMetrics _metrics;
  EventLoop _ev;
  std::atomic<bool> _stopped{false};

  void _listenerInit();

  void _sslListenerInit();
  void _initSSL();
  void _loadSSLCertificates();
  void _checkSSLCertUpdated();
};

} // namespace eventhub
