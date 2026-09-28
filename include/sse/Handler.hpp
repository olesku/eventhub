#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Forward.hpp"

namespace eventhub {
namespace sse {

class Handler final {
public:
  static void handleRequest(HandlerContext& ctx, const http::Request& request);

private:
  Handler() {}
  ~Handler() {}
};

} // namespace sse
} // namespace eventhub
