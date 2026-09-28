#pragma once

#include <functional>

namespace eventhub::http {

enum class ParseError {
  INVALID_REQUEST,
  REQUEST_TOO_LARGE
};

class Request;

struct ParserCallbacks {
  std::function<void(const Request&)> onRequest;
  std::function<void(ParseError)> onError;
};

} // namespace eventhub::http
