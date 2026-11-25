#include "websocket/Response.hpp"

#include <cstdint>
#include <string_view>

#include "PublishedEvent.hpp"
#include "websocket/Compression.hpp"

namespace eventhub::websocket {
namespace {

void sendFrame(Connection& conn, FrameType type, bool compressed, std::string_view prefix, std::string_view suffix) {
  const std::uint64_t size = prefix.size() + suffix.size();
  char header[10];
  header[0] = static_cast<char>(0x80 | (compressed ? 0x40 : 0) | static_cast<std::uint8_t>(type));
  std::size_t headerSize = 2;
  if (size < 126) {
    header[1] = static_cast<char>(size);
  } else if (size <= 65535) {
    header[1] = 126;
    header[2] = static_cast<char>(size >> 8);
    header[3] = static_cast<char>(size);
    headerSize = 4;
  } else {
    header[1] = 127;
    for (unsigned i = 0; i < 8; ++i) {
      header[2 + i] = static_cast<char>(size >> (56 - 8 * i));
    }
    headerSize = 10;
  }

  conn.write({std::string_view(header, headerSize), prefix, suffix});
}

} // namespace

void Response::sendData(ConnectionPtr conn, const std::string& data, FrameType frameType) {
  sendFrame(*conn, frameType, false, {}, data);
}

void Response::sendCompressibleText(Connection& conn, const std::string& data) {
  if (conn.hasWebSocketCompression()) {
    const auto compressed = compressMessage(data);
    if (!compressed.empty() && compressed.size() < data.size()) {
      sendFrame(conn, FrameType::TEXT_FRAME, true, {}, compressed);
      return;
    }
  }
  sendFrame(conn, FrameType::TEXT_FRAME, false, {}, data);
}

void Response::sendEvent(Connection& conn, const jsonrpcpp::Id& id, const PublishedEvent& event) {
  const auto prefix = R"({"id":)" + id.to_json().dump();
  const auto& suffix = event.websocketSuffix();
  if (conn.hasWebSocketCompression()) {
    const auto& compressed = event.compressedWebsocketSuffix();
    if (!compressed.empty() && compressed.size() + storedPrefixOverhead(prefix.size()) < suffix.size()) {
      // Store the subscriber-specific ID in byte-aligned blocks so every
      // subscriber can use the same compressed, context-independent suffix.
      sendFrame(conn, FrameType::TEXT_FRAME, true, encodeStoredPrefix(prefix), compressed);
      return;
    }
  }
  sendFrame(conn, FrameType::TEXT_FRAME, false, prefix, suffix);
}

} // namespace eventhub::websocket
