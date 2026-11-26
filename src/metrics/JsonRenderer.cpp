#include <stdint.h>
#include <string>
#include <initializer_list>
#include <vector>

#include "jwt/json/json.hpp"
#include "metrics/JsonRenderer.hpp"
#include "metrics/Types.hpp"
#include "Server.hpp"

namespace eventhub {
namespace metrics {

const std::string JsonRenderer::RenderMetrics(Server* server) {
  auto metrics = server->getAggregatedMetrics();

  nlohmann::json j;

  j["worker_count"]                = metrics.worker_count;
  j["publish_count"]               = metrics.publish_count;
  j["redis_connection_fail_count"] = metrics.redis_connection_fail_count;
  j["redis_publish_delay_ms"]      = metrics.redis_publish_delay_ms;

  j["current_connections_count"] = metrics.current_connections_count;
  j["total_connect_count"]       = metrics.total_connect_count;
  j["total_disconnect_count"]    = metrics.total_disconnect_count;
  j["eventloop_delay_ms"]        = metrics.eventloop_delay_ms;

  j["websocket_sent_bytes"]                  = metrics.websocket_sent_bytes;
  j["websocket_sent_uncompressed_bytes"]     = metrics.websocket_sent_uncompressed_bytes;
  j["websocket_received_bytes"]              = metrics.websocket_received_bytes;
  j["websocket_received_uncompressed_bytes"] = metrics.websocket_received_uncompressed_bytes;
  j["websocket_connections_count"]           = metrics.websocket_connections_count;
  j["compression_connections_count"]         = metrics.compression_connections_count;
  j["inflation_limit_rejected_count"]        = metrics.inflation_limit_rejected_count;

  return j.dump(4) + "\r\n";
}

} // namespace metrics
} // namespace eventhub
