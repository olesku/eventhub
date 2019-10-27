#ifndef EVENTHUB_SERVER_HPP
#define EVENTHUB_SERVER_HPP

#include "connection_worker.hpp"
#include "worker.hpp"
#include "redis.hpp"
#include <memory>
#include <mutex>

using namespace std;

namespace eventhub {
class Server {
public:
  Server(const string redisHost, int redisPort);
  ~Server();

  void start();
  void stop();
  const int get_server_socket();
  Worker* getWorker();
  void publish(const std::string topic_name, const std::string data);

  inline Redis& getRedis() { return redis; }
private:
  int _server_socket;
  WorkerGroup<Worker> _connection_workers;
  WorkerGroup<Worker>::iterator _cur_worker;
  std::mutex _publish_lock;
  Redis redis;
};
} // namespace eventhub

#endif
