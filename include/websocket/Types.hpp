#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace eventhub::websocket {

enum class FrameType : std::uint8_t {
  CONTINUATION_FRAME = 0x0,
  TEXT_FRAME         = 0x1,
  BINARY_FRAME       = 0x2,
  CLOSE_FRAME        = 0x8,
  PING_FRAME         = 0x9,
  PONG_FRAME         = 0xA
};

enum class ParserError {
  RESERVED_BITS_SET,
  INVALID_OPCODE,
  INVALID_CONTINUATION,
  FRAGMENTED_CONTROL,
  CONTROL_TOO_LONG,
  MASK_REQUIRED,
  NON_CANONICAL_LENGTH,
  INVALID_LENGTH,
  MESSAGE_TOO_BIG,
  INVALID_CLOSE_PAYLOAD,
  INVALID_UTF8
};

[[nodiscard]] std::string_view errorMessage(ParserError error) noexcept;

struct ParserCallbacks {
  // Called once per complete text/binary message or control frame. The parser
  // owns the payload; copy it if it must outlive this synchronous callback.
  std::function<void(FrameType, const std::string&)> onMessage;
  // Called at most once. A protocol error permanently stops this parser.
  std::function<void(ParserError)> onError;
};

} // namespace eventhub::websocket
