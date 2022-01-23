#pragma once

#include <atomic>
#include <cstdint>
#include <vector>

namespace eventhub {
namespace metrics {

struct WorkerMetrics {
  std::atomic<unsigned long> current_connections_count{0};
  std::atomic<unsigned long long> total_connect_count{0};
  std::atomic<unsigned long long> total_disconnect_count{0};
  std::atomic<unsigned long> eventloop_delay_ms{0};
};

struct ServerMetrics {
  std::atomic<unsigned long> server_start_unixtime{0};
  std::atomic<unsigned int> worker_count{0};
  std::atomic<unsigned long long> publish_count{0};
  std::atomic<unsigned int> redis_connection_fail_count{0};
  std::atomic<unsigned long> redis_publish_delay_ms{0};
};

struct AggregatedMetrics {
  AggregatedMetrics() : server_start_unixtime(0),
                        worker_count(0),
                        publish_count(0),
                        redis_connection_fail_count(0),
                        redis_publish_delay_ms(0),
                        current_connections_count(0),
                        total_connect_count(0),
                        total_disconnect_count(0),
                        eventloop_delay_ms(0){};

  unsigned long server_start_unixtime;
  unsigned int worker_count;
  unsigned long long publish_count;
  unsigned int redis_connection_fail_count;
  unsigned long redis_publish_delay_ms;

  unsigned long current_connections_count;
  unsigned long long total_connect_count;
  unsigned long long total_disconnect_count;
  unsigned long eventloop_delay_ms;
};

} // namespace metrics
} // namespace eventhub


