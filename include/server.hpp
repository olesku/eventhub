#ifndef EVENTHUB_SERVER_HPP
#define EVENTHUB_SERVER_HPP

#include "connection_worker.hpp"
#include "redis_subscriber.hpp"
#include "worker.hpp"
#include <memory>
#include <mutex>

using namespace std;

namespace eventhub {
class Server {
public:
  Server();
  ~Server();

  void start();
  void stop();
  const int get_server_socket();
  Worker* getWorker();
  void publish(const std::string topic_name, const std::string data);

private:
  int _server_socket;
  WorkerGroup<Worker> _connection_workers;
  WorkerGroup<Worker>::iterator _cur_worker;
  std::mutex _publish_lock;
  RedisSubscriber _redis_subscriber;
};
} // namespace eventhub

#endif
