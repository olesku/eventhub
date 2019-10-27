#ifndef EVENTHUB_CONNECTION_WORKER_HPP
#define EVENTHUB_CONNECTION_WORKER_HPP

#include "Connection.hpp"
#include "EventLoop.hpp"
#include "TopicManager.hpp"
#include "Worker.hpp"
#include <memory>
#include <mutex>
#include <vector>

namespace eventhub {
class Server; // Forward declaration.

typedef std::unordered_map<unsigned int, std::shared_ptr<Connection>> connection_list_t;

class Worker : public WorkerBase {
public:
  Worker(Server* srv);
  ~Worker();

  Server* getServer() { return _server; };
  void subscribeConnection(std::shared_ptr<Connection>& conn, const string& topicFilterName);
  void publish(const string& topicName, const string& data);

private:
  Server* _server;
  int _epoll_fd;
  EventLoop _ev;
  connection_list_t _connection_list;
  std::mutex _connection_list_mutex;
  TopicManager _topic_manager;

  void _acceptConnection();
  void _addConnection(int fd, struct sockaddr_in* csin);
  void _removeConnection(const connection_list_t::iterator& it);
  void _read(std::shared_ptr<Connection>& conn);

  void workerMain();
};
} // namespace eventhub

#endif
