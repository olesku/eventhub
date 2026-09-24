#pragma once

#include <cstddef>
#include <functional>

namespace eventhub::http {

enum class ParseError {
  INVALID_REQUEST,
  REQUEST_TOO_LARGE
};

enum class ParseStatus {
  NEED_MORE,
  COMPLETE,
  FAILED
};

struct ParseResult {
  std::size_t consumed;
  ParseStatus status;
};

class Request;

struct ParserCallbacks {
  std::function<void(const Request&)> onRequest;
  std::function<void(ParseError)> onError;
};

} // namespace eventhub::http
