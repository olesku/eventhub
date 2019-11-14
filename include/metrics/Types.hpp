#ifndef INCLUDE_METRICS_TYPES_HPP_
#define INCLUDE_METRICS_TYPES_HPP_

#include <vector>
#include <atomic>

namespace eventhub {
namespace metrics {

struct WorkerMetrics {
  std::atomic<unsigned long> current_connections_count{0};
  std::atomic<unsigned long long> total_connect_count{0};
  std::atomic<unsigned long long> total_disconnect_count{0};
  std::atomic<double> eventloop_delay_ms{0};
};

struct ServerMetrics {
  std::atomic<unsigned long> server_start_unixtime{0};
  std::atomic<unsigned int> worker_count{0};
  std::atomic<unsigned long long> publish_count{0};
  std::atomic<unsigned int> redis_connection_fail_count{0};
  std::atomic<double> redis_publish_delay_ms{0};
};

struct Metrics {
  std::vector<WorkerMetrics> worker_metrics;
  ServerMetrics server_metrics;
};

} // statisics
} // eventhub

#endif // INCLUDE_METRICS_TYPES_HPP_

