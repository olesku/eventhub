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
  static void HandleRequest(HandlerContext& ctx, http::Parser* req);

private:
  Handler() {}
  ~Handler() {}
};

} // namespace sse
} // namespace eventhub


