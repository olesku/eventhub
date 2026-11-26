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

  // WebSocket message payload as transferred, and what the same messages
  // take without permessage-deflate. The ratio between the two is the
  // compression ratio; they are equal for connections without compression.
  std::atomic<unsigned long long> websocket_sent_bytes{0};
  std::atomic<unsigned long long> websocket_sent_uncompressed_bytes{0};
  std::atomic<unsigned long long> websocket_received_bytes{0};
  std::atomic<unsigned long long> websocket_received_uncompressed_bytes{0};

  // Connections that are WebSocket right now, and how many of those have
  // negotiated permessage-deflate.
  std::atomic<unsigned long> websocket_connections_count{0};
  std::atomic<unsigned long> compression_connections_count{0};

  // Messages refused because they expanded more than the allowed ratio.
  std::atomic<unsigned long long> inflation_limit_rejected_count{0};
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
                        eventloop_delay_ms(0),
                        websocket_sent_bytes(0),
                        websocket_sent_uncompressed_bytes(0),
                        websocket_received_bytes(0),
                        websocket_received_uncompressed_bytes(0),
                        websocket_connections_count(0),
                        compression_connections_count(0),
                        inflation_limit_rejected_count(0){};

  unsigned long server_start_unixtime;
  unsigned int worker_count;
  unsigned long long publish_count;
  unsigned int redis_connection_fail_count;
  unsigned long redis_publish_delay_ms;

  unsigned long current_connections_count;
  unsigned long long total_connect_count;
  unsigned long long total_disconnect_count;
  unsigned long eventloop_delay_ms;

  unsigned long long websocket_sent_bytes;
  unsigned long long websocket_sent_uncompressed_bytes;
  unsigned long long websocket_received_bytes;
  unsigned long long websocket_received_uncompressed_bytes;
  unsigned long websocket_connections_count;
  unsigned long compression_connections_count;
  unsigned long long inflation_limit_rejected_count;
};

} // namespace metrics
} // namespace eventhub


