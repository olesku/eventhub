#include "websocket/Parser.hpp"

#include <algorithm>
#include <utility>

namespace eventhub::websocket {
namespace {

// Header states call this only after parse() has checked that input is nonempty.
std::uint8_t takeByte(std::string_view& input) noexcept {
  const auto byte = static_cast<std::uint8_t>(input.front());
  input.remove_prefix(1);
  return byte;
}

// Validate the reassembled message, since a UTF-8 sequence can cross both TCP
// reads and WebSocket fragments. Reject overlong encodings and non-scalars.
bool isValidUtf8(std::string_view text) noexcept {
  std::uint32_t codepoint = 0;
  std::uint32_t minimum   = 0;
  unsigned remaining      = 0;

  for (char value : text) {
    const auto byte = static_cast<unsigned char>(value);
    if (remaining != 0) {
      if ((byte & 0xc0) != 0x80) {
        return false;
      }
      codepoint = (codepoint << 6) | (byte & 0x3f);
      if (--remaining == 0 && (codepoint < minimum || codepoint > 0x10ffff ||
                               (codepoint >= 0xd800 && codepoint <= 0xdfff))) {
        return false;
      }
    } else if (byte <= 0x7f) {
      continue;
    } else if (byte >= 0xc2 && byte <= 0xdf) {
      codepoint = byte & 0x1f;
      minimum   = 0x80;
      remaining = 1;
    } else if (byte >= 0xe0 && byte <= 0xef) {
      codepoint = byte & 0x0f;
      minimum   = 0x800;
      remaining = 2;
    } else if (byte >= 0xf0 && byte <= 0xf4) {
      codepoint = byte & 0x07;
      minimum   = 0x10000;
      remaining = 3;
    } else {
      return false;
    }
  }
  return remaining == 0;
}

bool isValidCloseCode(std::uint16_t code) noexcept {
  // Registered protocol codes, plus the application/private-use ranges.
  // 1004, 1005, 1006 and 1015 must never appear on the wire.
  return (code >= 1000 && code <= 1003) || (code >= 1007 && code <= 1014) ||
         (code >= 3000 && code <= 4999);
}

} // namespace

std::string_view errorMessage(ParserError error) noexcept {
  switch (error) {
    case ParserError::RESERVED_BITS_SET:
      return "reserved frame bits are set";
    case ParserError::INVALID_OPCODE:
      return "invalid frame opcode";
    case ParserError::INVALID_CONTINUATION:
      return "invalid message fragmentation sequence";
    case ParserError::FRAGMENTED_CONTROL:
      return "control frames cannot be fragmented";
    case ParserError::CONTROL_TOO_LONG:
      return "control frame exceeds 125 bytes";
    case ParserError::MASK_REQUIRED:
      return "client frames must be masked";
    case ParserError::NON_CANONICAL_LENGTH:
      return "payload length is not minimally encoded";
    case ParserError::INVALID_LENGTH:
      return "payload length exceeds 63 bits";
    case ParserError::MESSAGE_TOO_BIG:
      return "message exceeds the configured size limit";
    case ParserError::INVALID_CLOSE_PAYLOAD:
      return "invalid close frame payload";
    case ParserError::INVALID_UTF8:
      return "invalid UTF-8 payload";
  }
  return "unknown WebSocket parser error";
}

Parser::Parser(std::size_t maxMessageSize, ParserCallbacks callbacks) : _callbacks(std::move(callbacks)), _max_message_size(std::min(maxMessageSize, std::string{}.max_size())) {}

void Parser::setCallbacks(ParserCallbacks callbacks) {
  _callbacks = std::move(callbacks);
}

ParseResult Parser::parse(std::string_view input) {
  const auto inputSize = input.size();
  try {
    while (!input.empty()) {
      switch (_state) {
        case State::OPCODE:
          _readOpcode(takeByte(input));
          break;
        case State::LENGTH:
          _readLength(takeByte(input));
          break;
        case State::EXTENDED_LENGTH: {
          const auto byte = takeByte(input);
          // RFC 6455 lengths are unsigned 63-bit integers, not arbitrary uint64s.
          if (_length_bytes_remaining == 8 && (byte & 0x80) != 0) {
            _fail(ParserError::INVALID_LENGTH);
            break;
          }
          _bytes_remaining = (_bytes_remaining << 8) | byte;
          if (--_length_bytes_remaining == 0) {
            _finishLength();
          }
          break;
        }
        case State::MASK:
          _mask[_mask_position++] = takeByte(input);
          if (_mask_position == _mask.size()) {
            _mask_position = 0;
            _state         = State::PAYLOAD;
            // Empty frames complete at the last mask byte, even if this read
            // ends there. They must not wait for another byte to arrive.
            if (_bytes_remaining == 0) {
              _finishFrame();
            }
          }
          break;
        case State::PAYLOAD:
          _readPayload(input);
          break;
        case State::CLOSED:
        case State::FAILED:
          return {inputSize - input.size(),
                  _state == State::CLOSED ? ParseStatus::CLOSED : ParseStatus::FAILED};
      }
    }
  } catch (...) {
    // The caller cannot recover the consumed byte count after a callback or
    // allocation throws. Preserve the original exception and stop this stream.
    _state = State::FAILED;
    throw;
  }
  const auto status = _state == State::CLOSED   ? ParseStatus::CLOSED
                      : _state == State::FAILED ? ParseStatus::FAILED
                                                : ParseStatus::ACTIVE;
  return {inputSize - input.size(), status};
}

void Parser::_readOpcode(std::uint8_t byte) {
  if ((byte & 0x70) != 0) {
    return _fail(ParserError::RESERVED_BITS_SET);
  }

  _frame_type = static_cast<FrameType>(byte & 0x0f);
  _final      = (byte & 0x80) != 0;
  _control    = (byte & 0x08) != 0;

  switch (_frame_type) {
    case FrameType::CONTINUATION_FRAME:
      if (!_fragmented) {
        return _fail(ParserError::INVALID_CONTINUATION);
      }
      break;
    case FrameType::TEXT_FRAME:
    case FrameType::BINARY_FRAME:
      if (_fragmented) {
        return _fail(ParserError::INVALID_CONTINUATION);
      }
      _message_type = _frame_type;
      _message.clear();
      break;
    case FrameType::CLOSE_FRAME:
    case FrameType::PING_FRAME:
    case FrameType::PONG_FRAME:
      if (!_final) {
        return _fail(ParserError::FRAGMENTED_CONTROL);
      }
      _control_payload.clear();
      break;
    default:
      return _fail(ParserError::INVALID_OPCODE);
  }
  _state = State::LENGTH;
}

void Parser::_readLength(std::uint8_t byte) {
  if ((byte & 0x80) == 0) {
    return _fail(ParserError::MASK_REQUIRED);
  }
  _length_tag = byte & 0x7f;
  if (_control && _length_tag > 125) {
    return _fail(ParserError::CONTROL_TOO_LONG);
  }

  _bytes_remaining = 0;
  if (_length_tag >= 126) {
    _length_bytes_remaining = _length_tag == 126 ? 2 : 8;
    _state                  = State::EXTENDED_LENGTH;
  } else {
    _bytes_remaining = _length_tag;
    _finishLength();
  }
}

void Parser::_finishLength() {
  if ((_length_tag == 126 && _bytes_remaining < 126) ||
      (_length_tag == 127 && _bytes_remaining < 65536)) {
    return _fail(ParserError::NON_CANONICAL_LENGTH);
  }
  // Check before allocating or narrowing the wire length to size_t. Subtraction
  // avoids overflow and includes all previously received message fragments.
  if (!_control && _bytes_remaining > _max_message_size - _message.size()) {
    return _fail(ParserError::MESSAGE_TOO_BIG);
  }
  if (_frame_type == FrameType::CLOSE_FRAME && _bytes_remaining == 1) {
    return _fail(ParserError::INVALID_CLOSE_PAYLOAD);
  }
  _mask_position = 0;
  _state         = State::MASK;
}

void Parser::_readPayload(std::string_view& input) {
  const auto count  = static_cast<std::size_t>(std::min<std::uint64_t>(_bytes_remaining, input.size()));
  auto& payload     = _control ? _control_payload : _message;
  const auto offset = payload.size();
  payload.append(input.data(), count);
  for (std::size_t i = 0; i < count; ++i) {
    payload[offset + i] = static_cast<char>(static_cast<unsigned char>(payload[offset + i]) ^ _mask[_mask_position]);
    _mask_position      = (_mask_position + 1) % _mask.size();
  }
  input.remove_prefix(count);
  _bytes_remaining -= count;
  if (_bytes_remaining == 0) {
    _finishFrame();
  }
}

void Parser::_finishFrame() {
  _state = State::OPCODE;
  if (!_control) {
    _fragmented = !_final;
    if (!_final) {
      return;
    }
    if (_message_type == FrameType::TEXT_FRAME && !isValidUtf8(_message)) {
      return _fail(ParserError::INVALID_UTF8);
    }
  } else if (_frame_type == FrameType::CLOSE_FRAME) {
    if (!_control_payload.empty()) {
      const auto code = static_cast<std::uint16_t>(
          (static_cast<unsigned char>(_control_payload[0]) << 8) |
          static_cast<unsigned char>(_control_payload[1]));
      if (!isValidCloseCode(code)) {
        return _fail(ParserError::INVALID_CLOSE_PAYLOAD);
      }
      if (!isValidUtf8(std::string_view(_control_payload).substr(2))) {
        return _fail(ParserError::INVALID_UTF8);
      }
    }
    // Close terminates the stream, including any unfinished data message and
    // frames that happened to arrive in the same socket read.
    _state = State::CLOSED;
    _message.clear();
  }

  if (_callbacks.onMessage) {
    _callbacks.onMessage(_control ? _frame_type : _message_type, _control ? _control_payload : _message);
  }
}

void Parser::_fail(ParserError error) {
  // Mark terminal before notifying the caller, so malformed trailing bytes can
  // never turn into application messages or trigger duplicate error callbacks.
  _state = State::FAILED;
  _message.clear();
  _control_payload.clear();
  if (_callbacks.onError) {
    _callbacks.onError(error);
  }
}

} // namespace eventhub::websocket
