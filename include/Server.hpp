#ifndef INCLUDE_SERVER_HPP_
#define INCLUDE_SERVER_HPP_

#include <memory>
#include <mutex>
#include <string>

#include "ConnectionWorker.hpp"
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
  const int getServerSocket();
  Worker* getWorker();
  void publish(const std::string topicName, const std::string data);
  inline Redis& getRedis() { return _redis; }

private:
  int _server_socket;
  WorkerGroup<Worker> _connection_workers;
  WorkerGroup<Worker>::iterator _cur_worker;
  std::mutex _connection_workers_lock;
  Redis _redis;
  metrics::ServerMetrics _metrics;
};

} // namespace eventhub

#endif // INCLUDE_SERVER_HPP_
