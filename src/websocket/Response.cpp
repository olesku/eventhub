#include "websocket/Response.hpp"

#include <cstdint>
#include <string_view>

#include "PublishedEvent.hpp"
#include "websocket/Compression.hpp"

namespace eventhub::websocket {
namespace {

/**
 * Write one unfragmented frame whose payload is prefix followed by suffix.
 * The two parts are given separately so that a payload shared between
 * subscribers doesn't have to be copied together with each one's own part.
 * @param conn Connection to write to.
 * @param type Frame opcode.
 * @param compressed Set RSV1, which marks a permessage-deflate payload.
 * @param prefix First part of the payload. Can be empty.
 * @param suffix Second part of the payload.
 */
void sendFrame(Connection& conn, FrameType type, bool compressed, std::string_view prefix, std::string_view suffix) {
  const std::uint64_t size = prefix.size() + suffix.size();
  char header[10];

  // FIN is always set. Server frames are not masked.
  header[0] = static_cast<char>(0x80 | (compressed ? 0x40 : 0) | static_cast<std::uint8_t>(type));

  // The length takes 7 bits, or 16 or 64 bits in network byte order when it
  // doesn't fit.
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

/**
 * Send data as a single uncompressed frame.
 * @param conn Connection to send to.
 * @param data Frame payload.
 * @param frameType Frame opcode.
 */
void Response::sendData(ConnectionPtr conn, const std::string& data, FrameType frameType) {
  sendFrame(*conn, frameType, false, {}, data);
}

/**
 * Send a text message, compressed if the client negotiated compression and
 * the message gets smaller from it.
 * @param conn Connection to send to.
 * @param data Message to send.
 */
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

/**
 * Send a published event to one subscriber.
 * The message is {"id":<subscription id> followed by a suffix that is the
 * same for every subscriber, and that the event builds only once.
 * @param conn Connection to send to.
 * @param id ID of the subscribe request the event belongs to.
 * @param event Event to send.
 */
void Response::sendEvent(Connection& conn, const jsonrpcpp::Id& id, const PublishedEvent& event) {
  const auto prefix  = R"({"id":)" + id.to_json().dump();
  const auto& suffix = event.websocketSuffix();

  if (conn.hasWebSocketCompression()) {
    const auto& compressed = event.compressedWebsocketSuffix();

    // Worth it only if the result, with the stored prefix, is smaller.
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
