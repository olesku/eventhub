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
  std::stringstream ss;

  gethostname(h_buf, sizeof(h_buf));

  for (auto& m : metricList) {
    // Add prefix to metric key if set in configuration.
    const std::string metricKey = !Config.getString("PROMETHEUS_METRIC_PREFIX").empty() ?
                                    (Config.getString("PROMETHEUS_METRIC_PREFIX") + "_" +  m.first) : m.first;

    ss << metricKey << "{instance=\"" << h_buf << ":" << Config.getInt("LISTEN_PORT") << "\"" << "} " << m.second << "\n";
  }

  return ss.str();
}

} // namespace eventhub
} // namespace metrics
