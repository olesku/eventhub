#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <fmt/format.h>
#include <openssl/pem.h>
#include <openssl/ssl3.h>
#include <openssl/tls1.h>
#include <spdlog/logger.h>
#include <stdint.h>
#include <stdio.h>
#include <sw/redis++/errors.h>
#include <unistd.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <initializer_list>
#include <list>
#include <thread>
#include <vector>

#include "Server.hpp"
#include "Common.hpp"
#include "Config.hpp"
#include "Util.hpp"
#include "jwt/json/json.hpp"
#include "metrics/Types.hpp"
#include "ConnectionWorker.hpp"
#include "KVStore.hpp"
#include "Logger.hpp"

namespace eventhub {

std::atomic<bool> stopEventhub{false};
std::atomic<bool> reloadEventhub{false};

unsigned const char alpn_protocol[] = "http/1.1";
unsigned int alpn_protocol_length   = 8;

Server::Server(Config& cfg)
    : _config(cfg), _server_socket(-1), _server_socket_ssl(-1), _ssl_enabled(false), _ssl_ctx(nullptr), _redis(cfg) {

}

Server::~Server() {
  LOG->trace("Server destructor called.");
  stop();
}

void Server::start() {
  if (config().get<bool>("disable_unsecure_listener") && !config().get<bool>("enable_ssl")) {
    LOG->critical("disable_unsecure_listener=true enable_ssl=false. You need to enable at least one listener.");
    exit(1);
  }

  // Ignore SIGPIPE.
  signal(SIGPIPE, SIG_IGN);

  if (!config().get<bool>("disable_unsecure_listener")) {
    _listenerInit();
  }

  if (config().get<bool>("disable_auth")) {
    LOG->warn("WARNING: Server is running with disable_auth=true. Everything is allowed by any client.");
  }

  if (config().get<bool>("enable_ssl")) {
    _initSSL();
  }

  // Start the connection workers.
  _connection_workers_lock.lock();

  unsigned int numWorkerThreads = config().get<int>("worker_threads") == 0 ? std::thread::hardware_concurrency() : config().get<int>("worker_threads");

  for (unsigned i = 0; i < numWorkerThreads; i++) {
    _connection_workers.addWorker(new Worker(this, i + 1));
  }

  _cur_worker = _connection_workers.begin();
  _connection_workers_lock.unlock();

  // Set up cronjob handler thread.
  auto cronJobs = std::thread([&]() {
    while (!stopEventhub) {
      _ev.process();
      auto delay = _ev.getNextTimerDelay();

      if (reloadEventhub) {
        reload();
      }

      // Sleep at most 100ms.
      if (delay.count() > 100) {
        delay = std::chrono::milliseconds(100);
      }

      std::this_thread::sleep_for(delay);
    }
  });

  _metrics.worker_count          = numWorkerThreads;
  _metrics.server_start_unixtime = Util::getTimeSinceEpoch();

  RedisMsgCallback cb = [&](const std::string& pattern, const std::string& topic, const std::string& msg) {
    // Calculate publish delay.
    if (topic == "$metrics$/system_unixtime") {
      try {
        auto j  = nlohmann::json::parse(msg);
        auto ts = stol(static_cast<std::string>(j["message"]), nullptr, 10);
        ;
        auto diff                       = Util::getTimeSinceEpoch() - ts;
        _metrics.redis_publish_delay_ms = (diff < 0) ? 0 : diff;
        return;
      } catch (...) {}

      return;
    }

    // Ask the workers to publish the message to our clients.
    publish(topic, msg);
    _metrics.publish_count++;
  };

  // Connect to redis.
  _redis.psubscribe("*", cb);

  // Instanciate KVStore.
  _kv_store = std::make_unique<KVStore>(_config, _redis);

  // Add cache purge cronjob if cache functionality is enabled.
  if (config().get<bool>("enable_cache")) {
    _ev.addTimer(
        CACHE_PURGER_INTERVAL_MS, [&](TimerCtx* ctx) {
          try {
            LOG->debug("Running cache purger.");
            auto purgedItems = _redis.purgeExpiredCacheItems();
            LOG->debug("Purged {} items.", purgedItems);
          } catch (...) {}
        },
        true);
  }

  // Add redis publish latency sampler cronjob.
  _ev.addTimer(
      METRIC_DELAY_SAMPLE_RATE_MS, [&](TimerCtx* ctx) {
        try {
          _redis.publishMessage("$metrics$/system_unixtime", "0", "$system$", std::to_string(Util::getTimeSinceEpoch()));
        } catch (...) {}
      },
      true);

  // Monitor ssl certificate and key for changes on disk and reload if updated.
  if (isSSL() && config().get<bool>("ssl_cert_auto_reload")) {
    _ev.addTimer(1000 * config().get<int>("ssl_cert_check_interval"), [&](TimerCtx* ctx) {
      _checkSSLCertUpdated();
    }, true);
  }

  bool reconnect = false;
  while (!stopEventhub) {
    try {
      if (reconnect) {
        _metrics.redis_connection_fail_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        reconnect = false;
        _redis.resetSubscribers();
        _redis.psubscribe("*", cb);
        LOG->info("Connection to Redis restored.");
      }

      _redis.consume();
    }

    catch (sw::redis::TimeoutError& e) {
      continue;
    }

    catch (sw::redis::Error& e) {
      reconnect = true;
      LOG->error("Failed to read from redis: {} Waiting 5 seconds before reconnect.", e.what());
    }
  }

  cronJobs.join();
}

void Server::reload() {
  LOG->info("Reloading Eventhub");
  reloadEventhub = false;
  _loadSSLCertificates();
}

int alpn_cb(SSL* ssl, const unsigned char** out, unsigned char* outlen,
            const unsigned char* in, unsigned int inlen, void* arg) {
  auto reqProto = fmt::format("{}", reinterpret_cast<const char*>(in));

  if (reqProto.find(reinterpret_cast<const char*>(alpn_protocol)) != std::string::npos) {
    *out    = alpn_protocol;
    *outlen = alpn_protocol_length;

    LOG->trace("HTTP/1.1 ALPN accepted ALPN: {}", reqProto);
    return SSL_TLSEXT_ERR_OK;
  }

  LOG->trace("HTTP/1.1 ALPN NOT accepted ALPN: {}", reqProto);
  return SSL_TLSEXT_ERR_NOACK;
}

void Server::_listenerInit() {
  // Set up listening socket.
  _server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (_server_socket == -1) {
    LOG->critical("Could not create server socket: {}.", strerror(errno));
    exit(1);
  }

  // Reuse port and address.
  int on = 1;
  setsockopt(_server_socket, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

  // Bind socket.
  struct sockaddr_in sin;
  memset(reinterpret_cast<char*>(&sin), '\0', sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port   = htons(config().get<int>("listen_port"));

  if (::bind(_server_socket, (struct sockaddr*)&sin, sizeof(sin)) == -1) {
    LOG->critical("Could not bind server socket to port {}: {}.", config().get<int>("listen_port"), strerror(errno));
    exit(1);
  }

  if (listen(_server_socket, 0) == -1) {
    LOG->critical("Could not listen on server socket: {}", strerror(errno));
    exit(1);
  }

  if (fcntl(_server_socket, F_SETFL, O_NONBLOCK) == -1) {
    LOG->critical("Failed set nonblock mode on server socket: {}.", strerror(errno));
    exit(1);
  }

  LOG->info("Listening on port {}.", config().get<int>("listen_port"));
}

void Server::_sslListenerInit() {
  _server_socket_ssl = socket(AF_INET, SOCK_STREAM, 0);
  if (_server_socket_ssl == -1) {
    LOG->critical("Could not create SSL server socket: {}.", strerror(errno));
    exit(1);
  }

  int on = 1;
  setsockopt(_server_socket_ssl, SOL_SOCKET, SO_REUSEADDR, (const char*)&on, sizeof(on));

  struct sockaddr_in sin;
  memset(reinterpret_cast<char*>(&sin), '\0', sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port   = htons(config().get<int>("ssl_listen_port"));

  if (::bind(_server_socket_ssl, (struct sockaddr*)&sin, sizeof(sin)) == -1) {
    LOG->critical("Could not bind server socket to port {}: {}.", config().get<int>("ssl_listen_port"), strerror(errno));
    exit(1);
  }

  if (listen(_server_socket_ssl, 0) == -1) {
    LOG->critical("Could not listen on server socket: {}", strerror(errno));
    exit(1);
  }

  if (fcntl(_server_socket_ssl, F_SETFL, O_NONBLOCK) == -1) {
    LOG->critical("Failed set nonblock mode on server socket: {}.", strerror(errno));
    exit(1);
  }

  LOG->info("Listening for SSL connections on port {}.", config().get<int>("ssl_listen_port"));
}

void Server::_initSSL() {
  _sslListenerInit();

  const SSL_METHOD* method = TLS_server_method();
  _ssl_ctx                 = SSL_CTX_new(method);

  if (_ssl_ctx == nullptr) {
    LOG->critical("Could not initialize SSL context: {}", Util::getSSLErrorString(ERR_get_error()));
    exit(1);
  }

  _loadSSLCertificates();

  SSL_CTX_set_ecdh_auto(_ssl_ctx, 1);
  SSL_CTX_set_options(_ssl_ctx, SSL_OP_CIPHER_SERVER_PREFERENCE);
  SSL_CTX_set_alpn_select_cb(_ssl_ctx, alpn_cb, nullptr);

  _ssl_enabled = true;
}

void Server::_loadSSLCertificates() {
  if (!config().get<bool>("enable_ssl")) {
    return;
  }

  const std::string caCert = config().get<std::string>("ssl_ca_certificate");
  const std::string cert   = config().get<std::string>("ssl_certificate");
  const std::string key    = config().get<std::string>("ssl_private_key");

  if (caCert.empty()) {
    SSL_CTX_set_default_verify_paths(_ssl_ctx);
  } else {
    if (SSL_CTX_load_verify_locations(_ssl_ctx, caCert.c_str(), nullptr) <= 0) {
      LOG->error("Error loading CA certificate: {}", Util::getSSLErrorString(ERR_get_error()));
      stop();
      exit(EXIT_FAILURE);
    }
  }

  if (SSL_CTX_use_certificate_chain_file(_ssl_ctx, cert.c_str()) <= 0) {
    LOG->error("Error loading certificate: {}", Util::getSSLErrorString(ERR_get_error()));
    stop();
    exit(EXIT_FAILURE);
  }

  if (SSL_CTX_use_PrivateKey_file(_ssl_ctx, key.c_str(), SSL_FILETYPE_PEM) <= 0) {
    LOG->error("Error loading private key: {}", Util::getSSLErrorString(ERR_get_error()));
    stop();
    exit(EXIT_FAILURE);
  }

  if (!SSL_CTX_check_private_key(_ssl_ctx)) {
    LOG->error("Error validating private key: {}", Util::getSSLErrorString(ERR_get_error()));
    stop();
    exit(EXIT_FAILURE);
  }

  _ssl_cert_md5_hash = Util::getFileMD5Sum(cert);
  _ssl_priv_key_md5_hash = Util::getFileMD5Sum(key);
}

void Server::_checkSSLCertUpdated() {
  assert(isSSL());

  try {
    const std::string ssl_cert_md5_hash = Util::getFileMD5Sum(config().get<std::string>("ssl_certificate"));
    const std::string ssl_priv_key_md5_hash = Util::getFileMD5Sum(config().get<std::string>("ssl_private_key"));
    bool reload = false;

    if (ssl_cert_md5_hash != _ssl_cert_md5_hash) {
      LOG->info("Change to certificate file " + config().get<std::string>("ssl_certificate") + " detected.");

      auto fp = fopen(config().get<std::string>("ssl_certificate").c_str(), "r");
      if (fp) {
        PEM_read_X509(fp, nullptr, nullptr, nullptr);
        auto err = ERR_get_error();

        if (err != 0) {
          LOG->info("Failed to validate certificate: " + Util::getSSLErrorString(err));
        } else {
          reload = true;
        }
        fclose(fp);
      }
    }

    if (ssl_priv_key_md5_hash != _ssl_priv_key_md5_hash) {
      LOG->info("Change to private key file " + config().get<std::string>("ssl_private_key") + " detected.");
      auto fp = fopen(config().get<std::string>("ssl_private_key").c_str(), "r");
      if (fp) {
        PEM_read_PrivateKey(fp, nullptr, nullptr, nullptr);
        auto err = ERR_get_error();

        if (err != 0) {
          LOG->info("Failed to validate private key: " + Util::getSSLErrorString(err));
        } else {
          reload = true;
        }
        fclose(fp);
      }
    }

    if (reload) {
      LOG->info("Reloading SSL certificate and private key.");
      _loadSSLCertificates();
    }
  } catch(...) {
    // getFileMD5Sum throws a runtime_error if file has been deleted.
    // TODO: We might want to handle this case, for now return no change.
  }
}

Worker* Server::getWorker() {
  std::lock_guard<std::mutex> lock(_connection_workers_lock);
  if (_cur_worker == _connection_workers.end()) {
    _cur_worker = _connection_workers.begin();
  }

  return (_cur_worker++)->get();
}

void Server::publish(const std::string& topicName, const std::string& data) {
  std::lock_guard<std::mutex> lock(_connection_workers_lock);
  for (auto& worker : _connection_workers.getWorkerList()) {
    worker->publish(topicName, data);
  }
}

void Server::stop() {
  close(_server_socket);

  if (isSSL())
    close(_server_socket_ssl);

  _connection_workers.killAndDeleteAll();
  if (_ssl_enabled && _ssl_ctx != nullptr) {
    SSL_CTX_free(_ssl_ctx);
  }
}

metrics::AggregatedMetrics Server::getAggregatedMetrics() {
  std::lock_guard<std::mutex> lock(_connection_workers_lock);
  metrics::AggregatedMetrics m;

  m.worker_count                = _metrics.worker_count.load();
  m.redis_publish_delay_ms      = _metrics.redis_publish_delay_ms.load();
  m.server_start_unixtime       = _metrics.server_start_unixtime.load();
  m.publish_count               = _metrics.publish_count.load();
  m.redis_connection_fail_count = _metrics.redis_connection_fail_count.load();

  for (auto& wrk : _connection_workers) {
    const auto& wrkM = wrk->getMetrics();
    m.current_connections_count += wrkM.current_connections_count.load();
    m.total_connect_count += wrkM.total_connect_count.load();
    m.total_disconnect_count += wrkM.total_disconnect_count.load();
    m.eventloop_delay_ms += wrkM.eventloop_delay_ms.load();
  }

  // Calculate avg eventloop delay accross workers.
  m.eventloop_delay_ms = (m.eventloop_delay_ms / _connection_workers.getWorkerList().size());

  return m;
}

} // namespace eventhub
