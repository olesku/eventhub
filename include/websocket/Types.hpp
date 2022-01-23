#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace eventhub {
namespace websocket {

enum class FrameType : uint8_t {
  CONTINUATION_FRAME = 0x0,
  TEXT_FRAME         = 0x1,
  BINARY_FRAME       = 0x2,
  CLOSE_FRAME        = 0x8,
  PING_FRAME         = 0x9,
  PONG_FRAME         = 0xA
};

enum class ParserStatus {
  PARSER_OK,
  MAX_DATA_FRAME_SIZE_EXCEEDED,
  MAX_CONTROL_FRAME_SIZE_EXCEEDED
};

using ParserCallback = std::function<void(ParserStatus status, FrameType frameType, const std::string& data)>;

} // namespace websocket
} // namespace eventhub


