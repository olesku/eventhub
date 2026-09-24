#pragma once

#include <atomic>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "Connection.hpp"
#include "EventLoop.hpp"
#include "EventhubBase.hpp"
#include "Forward.hpp"
#include "Worker.hpp"
#include "metrics/Types.hpp"

namespace eventhub {

class Worker final : public EventhubBase, public WorkerBase {
public:
  Worker(Server* srv, unsigned int workerId);
  ~Worker();

  TopicManager* getTopicManager() { return _topic_manager.get(); }

  void publish(const std::string& topicName, const std::string& data);
  void addTimer(int64_t delay, std::function<void(TimerCtx* ctx)> callback, bool repeat = false);
  bool enqueueAcceptedSocket(SocketHandle socket, const struct sockaddr_in& address, bool ssl);
  unsigned int getWorkerId() { return _workerId; }
  int getEpollFileDescriptor() { return _epoll_fd; }
  const metrics::WorkerMetrics& getMetrics() { return _metrics; }
  void addQueuedOutputBytes(std::size_t bytes) { _metrics.queued_output_bytes += bytes; }
  void removeQueuedOutputBytes(std::size_t bytes) { _metrics.queued_output_bytes -= bytes; }
  void setConnectionCongested(bool congested) {
    congested ? ++_metrics.congested_connections : --_metrics.congested_connections;
  }
  void recordSlowConsumerClose() { ++_metrics.slow_consumer_closes; }

private:
  struct PendingConnection {
    SocketHandle socket;
    struct sockaddr_in address;
    bool ssl;
  };

  static constexpr std::uint64_t TOKEN_KIND_MASK    = 0xf000000000000000ULL;
  static constexpr std::uint64_t TOKEN_ID_MASK      = 0x0fffffffffffffffULL;
  static constexpr std::uint64_t TOKEN_EVENT        = 0xf000000000000001ULL;
  static constexpr std::uint64_t TOKEN_TIMER        = 0xf000000000000002ULL;
  static constexpr std::uint64_t TOKEN_LISTENER     = 0xf000000000000003ULL;
  static constexpr std::uint64_t TOKEN_TLS_LISTENER = 0xf000000000000004ULL;

  unsigned int _workerId;
  Server* _server;
  int _epoll_fd;
  int _event_fd;
  int _timer_fd;
  std::unique_ptr<EventLoop> _ev;
  std::unordered_map<ConnectionId, ConnectionPtr> _connections;
  ConnectionId _next_connection_id{1};
  std::mutex _pending_connections_mutex;
  std::deque<PendingConnection> _pending_connections;
  std::atomic<bool> _accepting_commands{true};
  std::thread::id _owner_thread;
  std::unique_ptr<TopicManager> _topic_manager;
  metrics::WorkerMetrics _metrics;
  int64_t _ev_delay_sample_start;

  void _acceptConnection(bool ssl);
  ConnectionPtr _addConnection(PendingConnection pending);
  void _removeConnection(ConnectionPtr conn);
  void _initEventFd();
  void _closeEventFd();
  void _initTimerFd();
  void _closeTimerFd();
  void _signalWork();
  void _drainEventFd();
  void _drainTimerFd();
  void _armTimerFd();
  void _drainPendingConnections();
  void _closeConnections();
  void _assertOwnerThread() const;
  static std::uint64_t _connectionToken(ConnectionId id) { return id & TOKEN_ID_MASK; }

  void _workerMain() override;
  void _wakeForStop() override;
};

} // namespace eventhub
