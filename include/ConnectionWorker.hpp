#pragma once

#include <chrono>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <functional>

#include "Forward.hpp"
#include "metrics/Types.hpp"
#include "EventhubBase.hpp"
#include "EventLoop.hpp"
#include "Worker.hpp"
#include "Connection.hpp"

namespace eventhub {

typedef std::list<ConnectionPtr> ConnectionList;

class Worker final : public EventhubBase, public WorkerBase {
public:
  Worker(Server* srv, unsigned int workerId);
  ~Worker();

  TopicManager* getTopicManager() { return _topic_manager.get(); }

  void subscribeConnection(ConnectionPtr conn, const std::string& topicFilterName);
  void publish(const std::string& topicName, const std::string& data);
  void addTimer(int64_t delay, std::function<void(TimerCtx* ctx)> callback, bool repeat = false);
  unsigned int getWorkerId() { return _workerId; }
  int getEpollFileDescriptor() { return _epoll_fd; }
  const metrics::WorkerMetrics& getMetrics() { return _metrics; }

private:
  unsigned int _workerId;
  Server* _server;
  int _epoll_fd;
  std::unique_ptr<EventLoop> _ev;
  ConnectionList _connection_list;
  std::mutex _connection_list_mutex;
  std::unique_ptr<TopicManager> _topic_manager;
  metrics::WorkerMetrics _metrics;
  int64_t _ev_delay_sample_start;

  void _acceptConnection(bool ssl);
  ConnectionPtr _addConnection(int fd, struct sockaddr_in* csin, bool ssl);
  void _removeConnection(ConnectionPtr conn);
  void _read(ConnectionPtr conn);

  void _workerMain();
};

} // namespace eventhub


