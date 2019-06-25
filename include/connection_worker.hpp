#ifndef EVENTHUB_CONNECTION_WORKER_HPP
#define EVENTHUB_CONNECTION_WORKER_HPP

#include "connection.hpp"
#include "event_loop.hpp"
#include "topic_manager.hpp"
#include "worker.hpp"
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
  void subscribeConnection(std::shared_ptr<Connection>& conn, const string& topic_filter_name);
  void publish(const string& topic_name, const string& data);

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
