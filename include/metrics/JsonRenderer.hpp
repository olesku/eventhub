#ifndef INCLUDE_METRICS_JSON_RENDERER_HPP_
#define INCLUDE_METRICS_JSON_RENDERER_HPP_

#include <string>

#include "metrics/Types.hpp"

namespace eventhub {
namespace metrics {

class JsonRenderer {
public:
  static const std::string RenderMetrics(AggregatedMetrics&& metrics);
};

} // namespace metrics
} // namespace eventhub

#endif // INCLUDE_METRICS_JSON_RENDERER_HPP_
