#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "websocket/Types.hpp"

namespace eventhub::websocket {

// Incremental server-side RFC 6455 parser, without protocol extensions.
// Client frames must be masked. Input can end anywhere in a header or payload;
// it is never modified or retained. Only received payload bytes are buffered.
class Parser final {
public:
  // The limit applies to the entire reassembled data message. Control frames
  // have the separate protocol limit of 125 bytes.
  explicit Parser(std::size_t maxMessageSize, ParserCallbacks callbacks = {});

  // Replace handlers between parse calls without resetting the stream state.
  void setCallbacks(ParserCallbacks callbacks);

  // Callbacks are optional and run synchronously in wire order. Do not reenter
  // or destroy the parser from a callback, or use it concurrently. Exceptions
  // propagate to the caller and permanently stop parsing. After an error or a
  // close frame, further input is ignored.
  ParseResult parse(std::string_view input);

private:
  enum class State {
    OPCODE,
    LENGTH,
    EXTENDED_LENGTH,
    MASK,
    PAYLOAD,
    CLOSED,
    FAILED
  };

  void _readOpcode(std::uint8_t byte);
  void _readLength(std::uint8_t byte);
  void _finishLength();
  void _readPayload(std::string_view& input);
  void _finishFrame();
  void _fail(ParserError error);

  ParserCallbacks _callbacks;
  std::size_t _max_message_size;
  State _state                         = State::OPCODE;
  FrameType _frame_type                = FrameType::CONTINUATION_FRAME;
  FrameType _message_type              = FrameType::CONTINUATION_FRAME;
  bool _final                          = false;
  bool _control                        = false;
  bool _fragmented                     = false;
  std::uint64_t _bytes_remaining       = 0;
  std::uint8_t _length_tag             = 0;
  std::uint8_t _length_bytes_remaining = 0;
  std::array<std::uint8_t, 4> _mask{};
  std::size_t _mask_position = 0;

  // A control frame may arrive between data fragments without disturbing the
  // unfinished message. Keeping these separate also bounds control storage.
  std::string _message;
  std::string _control_payload;
};

} // namespace eventhub::websocket
