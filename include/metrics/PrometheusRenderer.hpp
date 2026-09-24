#pragma once

#include <string>

#include "Forward.hpp"
#include "metrics/Types.hpp"

namespace eventhub {

namespace metrics {

class PrometheusRenderer final {
public:
  static const std::string RenderMetrics(const AggregatedMetrics& metrics, const Config& config);
};

} // namespace metrics
} // namespace eventhub
