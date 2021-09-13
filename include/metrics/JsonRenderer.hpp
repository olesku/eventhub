#ifndef INCLUDE_METRICS_JSON_RENDERER_HPP_
#define INCLUDE_METRICS_JSON_RENDERER_HPP_

#include <string>

#include "Forward.hpp"
#include "metrics/Types.hpp"

namespace eventhub {
namespace metrics {

class JsonRenderer final {
public:
  static const std::string RenderMetrics(Server* server);
};

} // namespace metrics
} // namespace eventhub

#endif // INCLUDE_METRICS_JSON_RENDERER_HPP_
