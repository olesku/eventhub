#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Connection.hpp"
#include "ConnectionWorker.hpp"
#include "HandlerContext.hpp"
#include "http/Parser.hpp"

namespace eventhub {
class HandlerContext;
namespace http {
class Parser;
}  // namespace http

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


