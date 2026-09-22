#include <array>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "catch.hpp"
#include "websocket/Parser.hpp"

namespace eventhub::websocket {
namespace {

using Message = std::pair<FrameType, std::string>;

struct Recorder {
  std::vector<Message> messages;
  std::vector<ParserError> errors;

  ParserCallbacks callbacks() {
    return {
        [this](FrameType type, const std::string& data) { messages.emplace_back(type, data); },
        [this](ParserError error) { errors.push_back(error); }};
  }
};

std::string bytes(std::initializer_list<std::uint8_t> values) {
  return {values.begin(), values.end()};
}

// Independent wire fixture builder. Malformed headers and the RFC example below
// use literal bytes, so those checks do not depend on this helper's encoding.
std::string frame(FrameType type, std::string_view payload = {}, bool final = true,
                  std::array<std::uint8_t, 4> mask = {0x37, 0xfa, 0x21, 0x3d}) {
  std::string result(1, static_cast<char>((final ? 0x80 : 0) | static_cast<std::uint8_t>(type)));
  const std::uint64_t size = payload.size();
  if (size < 126) {
    result.push_back(static_cast<char>(0x80 | size));
  } else {
    result.push_back(static_cast<char>(size <= 65535 ? 0xfe : 0xff));
    for (int shift = size <= 65535 ? 8 : 56; shift >= 0; shift -= 8) {
      result.push_back(static_cast<char>((size >> shift) & 0xff));
    }
  }
  for (auto byte : mask) {
    result.push_back(static_cast<char>(byte));
  }
  for (std::size_t i = 0; i < payload.size(); ++i) {
    result.push_back(static_cast<char>(static_cast<unsigned char>(payload[i]) ^ mask[i % mask.size()]));
  }
  return result;
}

void feedChunks(Parser& parser, std::string_view input, std::size_t chunkSize) {
  while (!input.empty()) {
    const auto chunk = input.substr(0, chunkSize);
    parser.parse(chunk);
    input.remove_prefix(chunk.size());
  }
}

// Every error must be terminal, including when valid data follows in the same
// socket read. Repeat with bytewise input to exercise partial invalid headers.
void expectError(const std::string& input, ParserError expected, std::size_t limit = 100000) {
  for (auto chunkSize : {std::size_t{1}, input.size() + 100}) {
    CAPTURE(expected, limit, chunkSize);
    Recorder recorder;
    Parser parser(limit, recorder.callbacks());
    feedChunks(parser, input + frame(FrameType::TEXT_FRAME, "must not be delivered"), chunkSize);
    parser.parse(frame(FrameType::TEXT_FRAME, "nor this"));
    parser.parse({});
    REQUIRE(recorder.messages.empty());
    REQUIRE(recorder.errors == std::vector<ParserError>{expected});
  }
}

} // namespace

TEST_CASE("WebSocket parses the RFC masked example across every split", "[websocket]") {
  // RFC 6455 section 5.7: masked text message "Hello".
  const auto wire = bytes({0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d, 0x51, 0x58});
  for (std::size_t split = 0; split <= wire.size(); ++split) {
    CAPTURE(split);
    Recorder recorder;
    Parser parser(5, recorder.callbacks());
    auto input = wire;
    parser.parse({});
    parser.parse(std::string_view(input).substr(0, split));
    REQUIRE(recorder.messages.size() == (split == wire.size() ? 1 : 0));
    parser.parse(std::string_view(input).substr(split));
    REQUIRE(recorder.messages == std::vector<Message>{{FrameType::TEXT_FRAME, "Hello"}});
    REQUIRE(recorder.errors.empty());
    REQUIRE(input == wire);
  }
}

TEST_CASE("WebSocket handles all length encodings and binary bytes", "[websocket]") {
  for (std::size_t size : {0, 1, 125, 126, 127, 65535, 65536, 65537}) {
    CAPTURE(size);
    std::string payload(size, '\0');
    for (std::size_t i = 0; i < size; ++i) {
      payload[i] = static_cast<char>(i % 256);
    }
    const auto wire = frame(FrameType::BINARY_FRAME, payload);
    for (auto chunkSize : {std::size_t{1}, std::size_t{509}, wire.size()}) {
      CAPTURE(chunkSize);
      Recorder recorder;
      Parser parser(size, recorder.callbacks());
      feedChunks(parser, wire, chunkSize);
      REQUIRE(recorder.messages == std::vector<Message>{{FrameType::BINARY_FRAME, payload}});
      REQUIRE(recorder.errors.empty());
    }
  }
}

TEST_CASE("WebSocket waits for incomplete headers masks and payloads", "[websocket]") {
  for (std::size_t size : {0, 5, 126, 65536}) {
    const auto wire = frame(FrameType::BINARY_FRAME, std::string(size, 'x'));
    // Cover every header byte and the final payload byte without quadratic work
    // on the large frame.
    for (std::size_t prefix = 0; prefix < wire.size() && prefix < 14; ++prefix) {
      Recorder recorder;
      Parser parser(size, recorder.callbacks());
      parser.parse(std::string_view(wire).substr(0, prefix));
      parser.parse({});
      REQUIRE(recorder.messages.empty());
      REQUIRE(recorder.errors.empty());
      parser.parse(std::string_view(wire).substr(prefix, wire.size() - prefix - 1));
      REQUIRE(recorder.messages.empty());
      parser.parse(std::string_view(wire).substr(wire.size() - 1));
      REQUIRE(recorder.messages.size() == 1);
      REQUIRE(recorder.errors.empty());
    }
  }
}

TEST_CASE("WebSocket assembles fragments and delivers interleaved controls in order", "[websocket]") {
  for (auto type : {FrameType::TEXT_FRAME, FrameType::BINARY_FRAME}) {
    const auto wire = frame(type, "Hel", false) +
                      frame(FrameType::PING_FRAME, "ping") +
                      frame(FrameType::CONTINUATION_FRAME, {}, false) +
                      frame(FrameType::CONTINUATION_FRAME, "lo ", false, {1, 2, 3, 4}) +
                      frame(FrameType::PONG_FRAME, "pong") +
                      frame(FrameType::CONTINUATION_FRAME, "world", true, {0, 0, 0, 0}) +
                      frame(type, "next", false) + frame(FrameType::CONTINUATION_FRAME) +
                      frame(FrameType::TEXT_FRAME, "last");
    for (std::size_t split = 0; split <= wire.size(); ++split) {
      Recorder recorder;
      Parser parser(11, recorder.callbacks());
      parser.parse(std::string_view(wire).substr(0, split));
      parser.parse(std::string_view(wire).substr(split));
      REQUIRE(recorder.messages == std::vector<Message>{
                                       {FrameType::PING_FRAME, "ping"}, {FrameType::PONG_FRAME, "pong"}, {type, "Hello world"}, {type, "next"}, {FrameType::TEXT_FRAME, "last"}});
      REQUIRE(recorder.errors.empty());
    }
  }
}

TEST_CASE("WebSocket delivers coalesced messages without retaining input", "[websocket]") {
  Recorder recorder;
  Parser parser(5, recorder.callbacks());
  auto input     = frame(FrameType::TEXT_FRAME, "first") + frame(FrameType::TEXT_FRAME, "last");
  const auto cut = input.size() - 2;
  parser.parse(std::string_view(input).substr(0, cut));
  REQUIRE(recorder.messages == std::vector<Message>{{FrameType::TEXT_FRAME, "first"}});
  const auto remaining = input.substr(cut);
  input.assign(input.size(), '\0');
  parser.parse(remaining);
  REQUIRE(recorder.messages == std::vector<Message>{{FrameType::TEXT_FRAME, "first"}, {FrameType::TEXT_FRAME, "last"}});
  REQUIRE(recorder.errors.empty());
}

TEST_CASE("WebSocket accepts empty messages and maximum sized control frames", "[websocket]") {
  for (auto type : {FrameType::TEXT_FRAME, FrameType::BINARY_FRAME, FrameType::PING_FRAME,
                    FrameType::PONG_FRAME, FrameType::CLOSE_FRAME}) {
    Recorder recorder;
    Parser parser(0, recorder.callbacks());
    feedChunks(parser, frame(type), 1);
    REQUIRE(recorder.messages == std::vector<Message>{{type, ""}});
    REQUIRE(recorder.errors.empty());
  }
  for (auto type : {FrameType::PING_FRAME, FrameType::PONG_FRAME, FrameType::CLOSE_FRAME}) {
    Recorder recorder;
    Parser parser(0, recorder.callbacks());
    auto payload = std::string(125, 'x');
    if (type == FrameType::CLOSE_FRAME) {
      payload.replace(0, 2, bytes({0x03, 0xe8}));
    }
    feedChunks(parser, frame(type, payload), 3);
    REQUIRE(recorder.messages == std::vector<Message>{{type, payload}});
    REQUIRE(recorder.errors.empty());
  }
}

TEST_CASE("WebSocket rejects reserved bits opcodes and unmasked client frames", "[websocket]") {
  for (std::uint8_t reserved : {0x10, 0x20, 0x40, 0x70}) {
    expectError(bytes({static_cast<std::uint8_t>(0x81 | reserved)}), ParserError::RESERVED_BITS_SET);
  }
  for (std::uint8_t opcode : {3, 4, 5, 6, 7, 11, 12, 13, 14, 15}) {
    expectError(bytes({static_cast<std::uint8_t>(0x80 | opcode)}), ParserError::INVALID_OPCODE);
  }
  for (std::uint8_t opcode : {1, 2, 8, 9, 10}) {
    expectError(bytes({static_cast<std::uint8_t>(0x80 | opcode), 0}), ParserError::MASK_REQUIRED);
  }
}

TEST_CASE("WebSocket rejects invalid fragmentation sequences", "[websocket]") {
  expectError(frame(FrameType::CONTINUATION_FRAME), ParserError::INVALID_CONTINUATION);
  for (auto type : {FrameType::TEXT_FRAME, FrameType::BINARY_FRAME}) {
    expectError(frame(type, "unfinished", false) + frame(type, "new"), ParserError::INVALID_CONTINUATION);
    expectError(frame(type, "unfinished", false) + frame(type, "new", false), ParserError::INVALID_CONTINUATION);

    Recorder recorder;
    Parser parser(100, recorder.callbacks());
    parser.parse(frame(type, "a", false) + frame(FrameType::CONTINUATION_FRAME, "b"));
    REQUIRE(recorder.messages == std::vector<Message>{{type, "ab"}});
    parser.parse(frame(FrameType::CONTINUATION_FRAME, "extra"));
    REQUIRE(recorder.messages.size() == 1);
    REQUIRE(recorder.errors == std::vector<ParserError>{ParserError::INVALID_CONTINUATION});
  }
  for (auto type : {FrameType::CLOSE_FRAME, FrameType::PING_FRAME, FrameType::PONG_FRAME}) {
    expectError(frame(type, {}, false), ParserError::FRAGMENTED_CONTROL);
    expectError(frame(type, std::string(126, 'x')), ParserError::CONTROL_TOO_LONG);
    expectError(frame(type, std::string(65536, 'x')), ParserError::CONTROL_TOO_LONG);
  }
}

TEST_CASE("WebSocket rejects noncanonical and invalid 64 bit lengths", "[websocket]") {
  for (std::uint8_t length : {0, 125}) {
    expectError(bytes({0x82, 0xfe, 0, length}), ParserError::NON_CANONICAL_LENGTH);
  }
  for (std::uint16_t length : {0, 125, 126, 65535}) {
    expectError(bytes({0x82, 0xff, 0, 0, 0, 0, 0, 0,
                       static_cast<std::uint8_t>(length >> 8), static_cast<std::uint8_t>(length & 0xff)}),
                ParserError::NON_CANONICAL_LENGTH);
  }
  expectError(bytes({0x82, 0xff, 0x80}), ParserError::INVALID_LENGTH);
  expectError(bytes({0x82, 0xff, 0xff}), ParserError::INVALID_LENGTH);
}

TEST_CASE("WebSocket bounds allocation using the declared full message size", "[websocket]") {
  // No payload or even mask bytes are needed to detect excessive lengths.
  expectError(bytes({0x81, 0x81}), ParserError::MESSAGE_TOO_BIG, 0);
  expectError(bytes({0x82, 0xfe, 1, 0}), ParserError::MESSAGE_TOO_BIG, 255);
  expectError(bytes({0x82, 0xff, 0, 0, 0, 1, 0, 0, 0, 0}), ParserError::MESSAGE_TOO_BIG);
  expectError(bytes({0x82, 0xff, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}), ParserError::MESSAGE_TOO_BIG);

  expectError(frame(FrameType::TEXT_FRAME, "abc", false) + bytes({0x80, 0x83}), ParserError::MESSAGE_TOO_BIG, 5);

  Recorder recorder;
  Parser parser(5, recorder.callbacks());
  parser.parse(frame(FrameType::TEXT_FRAME, "abc", false) + frame(FrameType::PING_FRAME, "control"));
  REQUIRE(recorder.messages == std::vector<Message>{{FrameType::PING_FRAME, "control"}});
  parser.parse(frame(FrameType::CONTINUATION_FRAME, "de"));
  parser.parse(frame(FrameType::TEXT_FRAME, "12345"));
  REQUIRE(recorder.messages == std::vector<Message>{
                                   {FrameType::PING_FRAME, "control"}, {FrameType::TEXT_FRAME, "abcde"}, {FrameType::TEXT_FRAME, "12345"}});
  REQUIRE(recorder.errors.empty());
}

TEST_CASE("WebSocket validates UTF-8 across fragments and permits arbitrary binary", "[websocket]") {
  const auto valid = bytes({0, 0x7f, 0xc2, 0x80, 0xdf, 0xbf, 0xe0, 0xa0, 0x80,
                            0xed, 0x9f, 0xbf, 0xee, 0x80, 0x80, 0xef, 0xbf, 0xbf,
                            0xf0, 0x90, 0x80, 0x80, 0xf4, 0x8f, 0xbf, 0xbf});
  for (std::size_t split = 0; split <= valid.size(); ++split) {
    Recorder recorder;
    Parser parser(valid.size(), recorder.callbacks());
    const auto wire = frame(FrameType::TEXT_FRAME, std::string_view(valid).substr(0, split), false) +
                      frame(FrameType::PING_FRAME, bytes({0xff})) +
                      frame(FrameType::CONTINUATION_FRAME, std::string_view(valid).substr(split));
    feedChunks(parser, wire, 1);
    REQUIRE(recorder.messages == std::vector<Message>{{FrameType::PING_FRAME, bytes({0xff})}, {FrameType::TEXT_FRAME, valid}});
    REQUIRE(recorder.errors.empty());
  }

  for (const auto& invalid : {
           bytes({0x80}), bytes({0xc0, 0xaf}), bytes({0xc1, 0xbf}), bytes({0xc2}), bytes({0xc2, 0x20}),
           bytes({0xe0, 0x80, 0x80}), bytes({0xed, 0xa0, 0x80}), bytes({0xe2, 0x82}),
           bytes({0xf0, 0x80, 0x80, 0x80}), bytes({0xf4, 0x90, 0x80, 0x80}), bytes({0xf5}), bytes({0xff})}) {
    expectError(frame(FrameType::TEXT_FRAME, invalid), ParserError::INVALID_UTF8);
    expectError(frame(FrameType::TEXT_FRAME, invalid, false) + frame(FrameType::CONTINUATION_FRAME), ParserError::INVALID_UTF8);
    expectError(frame(FrameType::CLOSE_FRAME, bytes({0x03, 0xe8}) + invalid), ParserError::INVALID_UTF8);
    Recorder recorder;
    Parser parser(invalid.size(), recorder.callbacks());
    parser.parse(frame(FrameType::BINARY_FRAME, invalid));
    REQUIRE(recorder.messages == std::vector<Message>{{FrameType::BINARY_FRAME, invalid}});
    REQUIRE(recorder.errors.empty());
  }
}

TEST_CASE("WebSocket validates close payloads and stops after close", "[websocket]") {
  expectError(bytes({0x88, 0x81}), ParserError::INVALID_CLOSE_PAYLOAD);
  for (std::uint16_t code : {0, 999, 1004, 1005, 1006, 1015, 1016, 2999, 5000, 65535}) {
    const auto payload = bytes({static_cast<std::uint8_t>(code >> 8), static_cast<std::uint8_t>(code & 0xff)});
    expectError(frame(FrameType::CLOSE_FRAME, payload), ParserError::INVALID_CLOSE_PAYLOAD);
  }
  for (std::uint16_t code : {1000, 1001, 1002, 1003, 1007, 1008, 1009, 1010, 1011, 1012, 1013, 1014, 3000, 4999}) {
    Recorder recorder;
    Parser parser(20, recorder.callbacks());
    const auto payload = bytes({static_cast<std::uint8_t>(code >> 8), static_cast<std::uint8_t>(code & 0xff)}) + "bye";
    parser.parse(frame(FrameType::TEXT_FRAME, "unfinished", false) + frame(FrameType::CLOSE_FRAME, payload) +
                 frame(FrameType::CONTINUATION_FRAME, "ignored") + frame(FrameType::PING_FRAME));
    parser.parse(frame(FrameType::TEXT_FRAME, "also ignored"));
    REQUIRE(recorder.messages == std::vector<Message>{{FrameType::CLOSE_FRAME, payload}});
    REQUIRE(recorder.errors.empty());
  }
}

TEST_CASE("WebSocket callbacks are optional and can be installed before parsing", "[websocket]") {
  Parser parser(std::numeric_limits<std::size_t>::max());
  REQUIRE_NOTHROW(parser.parse(frame(FrameType::TEXT_FRAME, "discarded")));
  Recorder recorder;
  parser.setCallbacks(recorder.callbacks());
  parser.parse(frame(FrameType::TEXT_FRAME, "received"));
  REQUIRE(recorder.messages == std::vector<Message>{{FrameType::TEXT_FRAME, "received"}});
  parser.setCallbacks({});
  REQUIRE_NOTHROW(parser.parse(bytes({0xf1})));
}

TEST_CASE("WebSocket propagates callback exceptions and stops parsing", "[websocket]") {
  Parser parser(100);
  ParserCallbacks callbacks;
  std::string input;

  SECTION("Message callback throws with another frame in the same read") {
    callbacks.onMessage = [](FrameType, const std::string&) {
      throw std::runtime_error("message handler failed");
    };
    input = frame(FrameType::TEXT_FRAME, "first") + frame(FrameType::TEXT_FRAME, "second");
  }

  SECTION("Error callback throws") {
    callbacks.onError = [](ParserError) {
      throw std::runtime_error("error handler failed");
    };
    input = bytes({0xf1});
  }

  parser.setCallbacks(std::move(callbacks));
  REQUIRE_THROWS_AS(parser.parse(input), std::runtime_error);

  // The caller cannot know how much of the read was consumed before the
  // exception. Changing callbacks must not allow that stream to resume.
  Recorder recorder;
  parser.setCallbacks(recorder.callbacks());
  parser.parse(frame(FrameType::TEXT_FRAME, "must not be delivered"));
  REQUIRE(recorder.messages.empty());
  REQUIRE(recorder.errors.empty());
}

} // namespace eventhub::websocket
