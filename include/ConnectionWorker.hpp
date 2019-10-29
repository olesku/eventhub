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

typedef std::unordered_map<unsigned int, ConnectionPtr> connection_list_t;

class Worker : public WorkerBase {
public:
  Worker(Server* srv, unsigned int workerId);
  ~Worker();

  inline Server* getServer() { return _server; };
  inline TopicManager& getTopicManager() { return _topic_manager; }

  void subscribeConnection(ConnectionPtr conn, const string& topicFilterName);
  void publish(const string& topicName, const string& data);
  void addTimer(int64_t delay, std::function<void(TimerCtx* ctx)> callback, bool repeat = false);
  unsigned int getWorkerId() { return _workerId; }

private:
  unsigned int _workerId;
  Server* _server;
  int _epoll_fd;
  EventLoop _ev;
  connection_list_t _connection_list;
  std::mutex _connection_list_mutex;
  TopicManager _topic_manager;

  void _acceptConnection();
  void _addConnection(int fd, struct sockaddr_in* csin);
  void _removeConnection(const connection_list_t::iterator& it);
  void _read(ConnectionPtr conn);

  void _workerMain();
};
} // namespace eventhub

#endif
