#include "Server.hpp"

#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <errno.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include <mutex>
#include <string>
#include <chrono>
#include <future>
#include <atomic>

#include "jwt/json/json.hpp"
#include "Common.hpp"
#include "Config.hpp"
#include "metrics/Types.hpp"
#include "Util.hpp"

std::atomic<bool> stopEventhub{false};

namespace eventhub {

Server::Server(const string redisHost, int redisPort, const std::string redisPassword, int redisPoolSize)
    :  _server_socket(-1), _ssl_server_socket(-1), _ssl_method(NULL), _ssl_ctx(NULL),
       _redis(redisHost, redisPort, redisPassword, redisPoolSize) {}

Server::~Server() {
  LOG->trace("Server destructor called.");
  stop();

  if (_ssl_ctx != NULL) {
    SSL_CTX_free(_ssl_ctx);
    EVP_cleanup();
  }
}

void Server::start() {
  // Ignore SIGPIPE.
  signal(SIGPIPE, SIG_IGN);

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
  sin.sin_port   = htons(Config.getInt("LISTEN_PORT"));

  if (::bind(_server_socket, (struct sockaddr*)&sin, sizeof(sin)) == -1) {
    LOG->critical("Could not bind server socket to port {}: {}.", Config.getInt("LISTEN_PORT"), strerror(errno));
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

  LOG->info("Listening on port {}.", Config.getInt("LISTEN_PORT"));

  if (Config.getBool("DISABLE_AUTH")) {
    LOG->warn("WARNING: Server is running with DISABLE_AUTH=true. Everything is allowed by any client.");
  }

  // Start the connection workers.
  _connection_workers_lock.lock();

  unsigned int numWorkerThreads = Config.getInt("WORKER_THREADS") == 0 ? std::thread::hardware_concurrency() : Config.getInt("WORKER_THREADS");

  for (unsigned i = 0; i < numWorkerThreads; i++) {
    _connection_workers.addWorker(new Worker(this, i + 1));
  }

  _cur_worker = _connection_workers.begin();
  _connection_workers_lock.unlock();


  // Set up cronjob handler thread.
  auto cronJobs = std::thread([&]() {
    while(!stopEventhub) {
      _ev.process();
      auto delay = _ev.getNextTimerDelay();

      // Sleep at most 100ms.
      if (delay.count() > 100) {
        delay = std::chrono::milliseconds(100);
      }

      std::this_thread::sleep_for(delay);
    }
  });

  _metrics.worker_count = numWorkerThreads;
  _metrics.server_start_unixtime = Util::getTimeSinceEpoch();

  _redis.setPrefix(Config.getString("REDIS_PREFIX"));

  RedisMsgCallback cb = [&](std::string pattern, std::string topic, std::string msg) {
    // Calculate publish delay.
    if (topic == "$metrics$/system_unixtime") {
      try {
        auto j = nlohmann::json::parse(msg);
        auto ts = stol(static_cast<std::string>(j["message"]), nullptr, 10);;
        auto diff = Util::getTimeSinceEpoch() - ts;
        _metrics.redis_publish_delay_ms = (diff < 0) ? 0 : diff;
        return;
      } catch(...) {}

      return;
    }

    // Ask the workers to publish the message to our clients.
    publish(topic, msg);
    _metrics.publish_count++;
  };

  // Connect to redis.
  _redis.psubscribe("*", cb);

  // Add cache purge cronjob if cache functionality is enabled.
  if (Config.getBool("ENABLE_CACHE")) {
    _ev.addTimer(CACHE_PURGER_INTERVAL_MS, [&](TimerCtx *ctx) {
      try {
        LOG->debug("Running cache purger.");
        auto purgedItems = _redis.purgeExpiredCacheItems();
        LOG->debug("Purged {} items.", purgedItems);
      } catch(...) {}
    }, true);
  }

  // Add redis publish latency sampler cronjob.
   _ev.addTimer(METRIC_DELAY_SAMPLE_RATE_MS, [&](TimerCtx *ctx) {
     try {
      _redis.publishMessage("$metrics$/system_unixtime", "0", to_string(Util::getTimeSinceEpoch()));
     } catch(...) {}
   }, true);

  bool reconnect = false;
  while (!stopEventhub) {
    try {
      if (reconnect) {
        _metrics.redis_connection_fail_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        reconnect = false;
        _redis.resetSubscribers();
        _redis.psubscribe("*", cb);
        LOG->info("Connection to Redis regained.");
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

Worker* Server::getWorker() {
  std::lock_guard<std::mutex> lock(_connection_workers_lock);
  if (_cur_worker == _connection_workers.end()) {
    _cur_worker = _connection_workers.begin();
  }

  return (_cur_worker++)->get();
}

void Server::publish(const string topicName, const string data) {
  std::lock_guard<std::mutex> lock(_connection_workers_lock);
  for (auto& worker : _connection_workers.getWorkerList()) {
    worker->publish(topicName, data);
  }
}

void Server::stop() {
  close(_server_socket);
  _connection_workers.killAndDeleteAll();
}

const int Server::getServerSocket() {
  return _server_socket;
}

metrics::AggregatedMetrics Server::getAggregatedMetrics() {
  std::lock_guard<std::mutex> lock(_connection_workers_lock);
  metrics::AggregatedMetrics m;

  m.worker_count = _metrics.worker_count.load();
  m.redis_publish_delay_ms = _metrics.redis_publish_delay_ms.load();
  m.server_start_unixtime =  _metrics.server_start_unixtime.load();
  m.publish_count = _metrics.publish_count.load();
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
