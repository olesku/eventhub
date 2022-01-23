#pragma once

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


