#pragma once

#include <string>

#include "metrics/Types.hpp"

namespace eventhub {
class Server;

namespace metrics {

class JsonRenderer final {
public:
  static const std::string RenderMetrics(Server* server);
};

} // namespace metrics
} // namespace eventhub


