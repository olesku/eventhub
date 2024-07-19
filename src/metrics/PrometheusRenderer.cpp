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

  std::vector<std::tuple<std::string, std::string, long long>> metricList = {
      {"worker_count", "gauge", metrics.worker_count},
      {"publish_count", "counter", metrics.publish_count},
      {"redis_connection_fail_count", "counter", metrics.redis_connection_fail_count},
      {"redis_publish_delay_ms", "histogram", metrics.redis_publish_delay_ms},

      {"current_connections_count", "gauge", metrics.current_connections_count},
      {"total_connect_count", "counter", metrics.total_connect_count},
      {"total_disconnect_count", "counter", metrics.total_disconnect_count},
      {"eventloop_delay_ms", "histogram", metrics.eventloop_delay_ms}};

  char h_buf[128] = {0};
  std::stringstream ss;

  gethostname(h_buf, sizeof(h_buf));

  for (const auto& metric : metricList) {
    const std::string& metricName = std::get<0>(metric);
    const std::string& metricType = std::get<1>(metric);
    const long long& metricValue   = std::get<2>(metric);

    // Output the type of each metric
    ss << "# TYPE " << metricName << " " << metricType << "\n";

    // Add prefix to metric key if set in configuration.
    const std::string metricKey = !config.get<std::string>("prometheus_metric_prefix").empty() ? (config.get<std::string>("prometheus_metric_prefix") + "_" + metricName) : metricName;

    ss << metricKey << "{instance=\"" << h_buf << ":" << config.get<int>("listen_port") << "\""
       << "} " << metricValue << "\n";
  }

  return ss.str();
}

} // namespace metrics
} // namespace eventhub
