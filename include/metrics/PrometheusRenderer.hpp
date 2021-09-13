#ifndef INCLUDE_METRICS_PROMETHEUS_RENDERER_HPP_
#define INCLUDE_METRICS_PROMETHEUS_RENDERER_HPP_

#include <string>

#include "metrics/Types.hpp"
#include "Forward.hpp"

namespace eventhub {
namespace metrics {

class PrometheusRenderer final {
public:
  static const std::string RenderMetrics(Server* server);
};

} // namespace metrics
} // namespace eventhub

#endif // INCLUDE_METRICS_PROMETHEUS_RENDERER_HPP_
