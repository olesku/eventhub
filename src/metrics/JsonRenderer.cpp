#include <string>

#include "jwt/json/json.hpp"
#include "metrics/JsonRenderer.hpp"
#include "metrics/Types.hpp"

namespace eventhub {
namespace metrics {

const std::string JsonRenderer::RenderMetrics(AggregatedMetrics&& metrics) {
  nlohmann::json j;

  j["worker_count"]                = metrics.worker_count;
  j["publish_count"]               = metrics.publish_count;
  j["redis_connection_fail_count"] = metrics.redis_connection_fail_count;
  j["redis_publish_delay_ms"]      = metrics.redis_publish_delay_ms;

  j["current_connections_count"] = metrics.current_connections_count;
  j["total_connect_count"]       = metrics.total_connect_count;
  j["total_disconnect_count"]    = metrics.total_disconnect_count;
  j["eventloop_delay_ms"]        = metrics.eventloop_delay_ms;

  return j.dump(4) + "\r\n";
}

} // namespace metrics
} // namespace eventhub
