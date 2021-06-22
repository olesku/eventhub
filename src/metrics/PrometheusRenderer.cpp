#include <unistd.h>

#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Server.hpp"
#include "Config.hpp"
#include "metrics/PrometheusRenderer.hpp"
#include "metrics/Types.hpp"

namespace eventhub {
namespace metrics {

const std::string PrometheusRenderer::RenderMetrics(Server* server) {
  auto  metrics = server->getAggregatedMetrics();
  auto& config  = server->config();

  std::vector<std::pair<std::string, long long>> metricList = {
      {"worker_count", metrics.worker_count},
      {"publish_count", metrics.publish_count},
      {"redis_connection_fail_count", metrics.redis_connection_fail_count},
      {"redis_publish_delay_ms", metrics.redis_publish_delay_ms},

      {"current_connections_count", metrics.current_connections_count},
      {"total_connect_count", metrics.total_connect_count},
      {"total_disconnect_count", metrics.total_disconnect_count},
      {"eventloop_delay_ms", metrics.eventloop_delay_ms}};

  char h_buf[128] = {0};
  std::stringstream ss;

  gethostname(h_buf, sizeof(h_buf));

  for (auto& m : metricList) {
    // Add prefix to metric key if set in configuration.
    const std::string metricKey = !config.get<std::string>("prometheus_metric_prefix").empty() ? (config.get<std::string>("prometheus_metric_prefix") + "_" + m.first) : m.first;

    ss << metricKey << "{instance=\"" << h_buf << ":" << config.get<int>("listen_port") << "\""
       << "} " << m.second << "\n";
  }

  return ss.str();
}

} // namespace metrics
} // namespace eventhub
