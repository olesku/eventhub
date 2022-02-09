#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace eventhub {
namespace http {

enum class RequestState {
  REQ_FAILED,
  REQ_INCOMPLETE,
  REQ_TO_BIG,
  REQ_OK
};

using ParserCallback = std::function<void(class Parser* req, RequestState state)>;

} // namespace http
} // namespace eventhub


