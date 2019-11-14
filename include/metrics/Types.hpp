#ifndef INCLUDE_METRICS_TYPES_HPP_
#define INCLUDE_METRICS_TYPES_HPP_

#include <vector>
#include <atomic>

namespace eventhub {
namespace metrics {

struct WorkerMetrics {
  void operator = (WorkerMetrics& w) {
    w.current_connections_count = current_connections_count.load();
    w.total_connect_count = total_connect_count.load();
    w.total_disconnect_count = total_disconnect_count.load();
    w.eventloop_delay_ms = eventloop_delay_ms.load();
  }

  WorkerMetrics(const WorkerMetrics& m) {
 
  };

  std::atomic<unsigned long> current_connections_count{0};
  std::atomic<unsigned long long> total_connect_count{0};
  std::atomic<unsigned long long> total_disconnect_count{0};
  std::atomic<double> eventloop_delay_ms{0};
};

struct ServerMetrics {
  void operator = (ServerMetrics& s) {
    s.server_start_unixtime = server_start_unixtime.load();
    s.worker_count = worker_count.load();
    s.publish_count = publish_count.load();
    s.redis_connection_fail_count = redis_connection_fail_count.load();
    s.redis_publish_delay_ms = redis_publish_delay_ms.load();
  }

  std::atomic<unsigned long> server_start_unixtime{0};
  std::atomic<unsigned int> worker_count{0};
  std::atomic<unsigned long long> publish_count{0};
  std::atomic<unsigned int> redis_connection_fail_count{0};
  std::atomic<double> redis_publish_delay_ms{0};
};

class Metrics {
  public:
  Metrics() {};
  ~Metrics() {};

  Metrics(Metrics& m) {
    m.server_metrics = server_metrics;
    m.worker_metrics = worker_metrics;
  }

  std::vector<WorkerMetrics> worker_metrics;
  ServerMetrics server_metrics;
};

} // statisics
} // eventhub

#endif // INCLUDE_METRICS_TYPES_HPP_

