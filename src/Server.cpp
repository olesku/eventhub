#include "Server.hpp"

#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>

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
    : _redis(redisHost, redisPort, redisPassword, redisPoolSize) {}

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
  memset(reinterpret_cast<char*>(&sin), '\0', sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_port   = htons(Config.getInt("LISTEN_PORT"));

  LOG_IF(FATAL, (::bind(_server_socket, (struct sockaddr*)&sin, sizeof(sin))) == -1) << "Could not bind server socket to port 8080";

  LOG_IF(FATAL, listen(_server_socket, 0) == -1) << "Call to listen() failed.";
  LOG_IF(FATAL, fcntl(_server_socket, F_SETFL, O_NONBLOCK) == -1) << "fcntl O_NONBLOCK on serversocket failed.";

  LOG(INFO) << "Listening on port " << Config.getInt("LISTEN_PORT") << ".";

  if (Config.getBool("DISABLE_AUTH")) {
    LOG(INFO) << "WARNING: Server is running with DISABLE_AUTH=true. Everything is allowed by any client.";
  }

  // Start the connection workers.
  _connection_workers_lock.lock();

  unsigned int numWorkerThreads = Config.getInt("WORKER_THREADS") == 0 ? std::thread::hardware_concurrency() : Config.getInt("WORKER_THREADS");

  for (unsigned i = 0; i < numWorkerThreads; i++) {
    _connection_workers.addWorker(new Worker(this, i + 1));
  }

  _cur_worker = _connection_workers.begin();
  _connection_workers_lock.unlock();

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

  // Sample Redis publish latency.
  auto redisSamplerThread = std::thread([&](){
    while(!stopEventhub) {
      try {
        _redis.publishMessage("$metrics$/system_unixtime", "0", to_string(Util::getTimeSinceEpoch()));
        std::this_thread::sleep_for(std::chrono::milliseconds(METRIC_DELAY_SAMPLE_RATE_MS));
      } catch(...) {}
    }
  });

  bool reconnect = false;
  while (!stopEventhub) {
    try {
      if (reconnect) {
        _metrics.redis_connection_fail_count++;
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        reconnect = false;
        _redis.resetSubscribers();
        _redis.psubscribe("*", cb);
        LOG(INFO) << "Connection to Redis regained.";
      }

      _redis.consume();
    }

    catch (sw::redis::TimeoutError) {
      continue;
    }

    catch (sw::redis::Error& e) {
      reconnect = true;
      LOG(ERROR) << "Failed to read from redis: " << e.what() << ". Waiting 5 seconds before reconnect.";
    }
  }

  redisSamplerThread.join();
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
