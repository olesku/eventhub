#ifndef INCLUDE_CONNECTIONWORKER_HPP_
#define INCLUDE_CONNECTIONWORKER_HPP_

#include <chrono>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <string>

#include "Forward.hpp"
#include "EventhubBase.hpp"
#include "Config.hpp"
#include "Connection.hpp"
#include "EventLoop.hpp"
#include "TopicManager.hpp"
#include "Worker.hpp"
#include "metrics/Types.hpp"

namespace eventhub {

typedef std::list<ConnectionPtr> ConnectionList;

class Worker final : public EventhubBase, public WorkerBase {
public:
  Worker(Server* srv, unsigned int workerId);
  ~Worker();

  inline TopicManager& getTopicManager() { return _topic_manager; }

  void subscribeConnection(ConnectionPtr conn, const string& topicFilterName);
  void publish(const string& topicName, const string& data);
  void addTimer(int64_t delay, std::function<void(TimerCtx* ctx)> callback, bool repeat = false);
  inline unsigned int getWorkerId() { return _workerId; }
  inline int getEpollFileDescriptor() { return _epoll_fd; }
  const metrics::WorkerMetrics& getMetrics() { return _metrics; }

private:
  unsigned int _workerId;
  Server* _server;
  int _epoll_fd;
  EventLoop _ev;
  ConnectionList _connection_list;
  std::mutex _connection_list_mutex;
  TopicManager _topic_manager;
  metrics::WorkerMetrics _metrics;
  int64_t _ev_delay_sample_start;

  void _acceptConnection();
  ConnectionPtr _addConnection(int fd, struct sockaddr_in* csin);
  void _removeConnection(ConnectionPtr conn);
  void _read(ConnectionPtr conn);

  void _workerMain();
};

} // namespace eventhub

#endif // INCLUDE_CONNECTIONWORKER_HPP_
