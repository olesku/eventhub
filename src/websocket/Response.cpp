#include "websocket/Response.hpp"

#include <cstdint>
#include <string_view>

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

FanoutMessage::FanoutMessage(const nlohmann::json& result) : _suffix(result.dump()) {
  _suffix.insert(0, R"(,"jsonrpc":"2.0","result":)");
  _suffix += '}';
}

void FanoutMessage::sendTo(Connection& conn, const jsonrpcpp::Id& id) const {
  const auto prefix = R"({"id":)" + id.to_json().dump();
  if (conn.hasWebSocketCompression()) {
    std::call_once(_compression_once, [this] {
      auto compressed = compressMessage(_suffix);
      if (compressed.size() < _suffix.size()) {
        _compressed_suffix.swap(compressed);
      }
    });
    if (!_compressed_suffix.empty() && _compressed_suffix.size() + storedPrefixOverhead(prefix.size()) < _suffix.size()) {
      // Store the subscriber-specific ID in byte-aligned blocks so every
      // subscriber can use the same compressed, context-independent suffix.
      sendFrame(conn, FrameType::TEXT_FRAME, true, encodeStoredPrefix(prefix), _compressed_suffix);
      return;
    }
  }
  sendFrame(conn, FrameType::TEXT_FRAME, false, prefix, _suffix);
}

} // namespace eventhub::websocket
