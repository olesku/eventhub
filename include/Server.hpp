#ifndef EVENTHUB_SERVER_HPP
#define EVENTHUB_SERVER_HPP

#include "ConnectionWorker.hpp"
#include "Redis.hpp"
#include "Worker.hpp"
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
};
} // namespace eventhub

#endif
