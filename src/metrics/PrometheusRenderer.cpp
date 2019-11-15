#include <unistd.h>

#include <string>
#include <sstream>
#include <vector>
#include <utility>

#include "Config.hpp"
#include "metrics/Types.hpp"
#include "metrics/PrometheusRenderer.hpp"

namespace eventhub {
namespace metrics {

/*
  unsigned long server_start_unixtime;
  unsigned int worker_count;
  unsigned long long publish_count;
  unsigned int redis_connection_fail_count;
  double redis_publish_delay_ms;

  unsigned long current_connections_count;
  unsigned long long total_connect_count;
  unsigned long long total_disconnect_count;
  unsigned int eventloop_delay_ms;

  metric_name [
  "{" label_name "=" `"` label_value `"` { "," label_name "=" `"` label_value `"` } [ "," ] "}"
] value [ timestamp ]
*/

const std::string PrometheusRenderer::RenderMetrics(AggregatedMetrics&& metrics) {
  std::vector<std::pair<std::string, long long>> metricList = {
    { "worker_count", metrics.worker_count },
    { "publish_count", metrics.publish_count },
    { "redis_connection_fail_count", metrics.redis_connection_fail_count },
    { "redis_publish_delay_ms", metrics.redis_publish_delay_ms },

    { "current_connections_count", metrics.current_connections_count },
    { "total_connect_count", metrics.total_connect_count },
    { "total_disconnect_count", metrics.total_disconnect_count },
    { "eventloop_delay_ms", metrics.eventloop_delay_ms }
  };

  char h_buf[128] = {0};

  gethostname(h_buf, sizeof(h_buf));

  std::stringstream ss;
  for (auto& m : metricList) {
    ss << m.first << "{instance=\"" << h_buf << ":" << Config.getInt("LISTEN_PORT") << "\"" << "} " << m.second << "\n";
  }

  return ss.str();
}

} // namespace eventhub
} // namespace metrics
