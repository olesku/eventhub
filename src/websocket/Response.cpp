#include "websocket/Response.hpp"

#include <arpa/inet.h>
#include <string.h>
#include <string>

#include "Common.hpp"
#include "websocket/Types.hpp"

namespace eventhub {
namespace websocket {
// TODO: This should be a static class instead of a namespace.
namespace response {

void sendFragment(ConnectionPtr conn, const std::string& fragment, uint8_t frameType, bool fin) {
  std::string sndBuf;
  char header[8];
  size_t headerSize   = 0;
  size_t fragmentSize = fragment.size();

  header[0] = fin << 7;
  header[0] = header[0] | (0xF & frameType);
  header[1] = 0x0 << 7; // No mask.

  if (fragmentSize < 126) {
    header[1]  = header[1] | fragmentSize;
    headerSize = 2;
  } else if (fragmentSize < 0xFFFF) {
    header[1]   = header[1] | 0x7E;
    uint16_t sz = htons(static_cast<uint16_t>(fragmentSize));
    memcpy(header + 2, &sz, sizeof(uint16_t));
    headerSize = 4;
  } else {
    header[1]   = header[1] | 0x7F;
    uint16_t sz = htonl(static_cast<uint16_t>(fragmentSize));
    memcpy(header + 2, &sz, sizeof(uint16_t));
    headerSize = 8;
  }

  sndBuf.insert(0, header, headerSize);
  sndBuf.append(fragment);
  conn->write(sndBuf);
}

void sendData(ConnectionPtr conn, const std::string& data, FrameType frameType) {
  size_t dataSize = data.size();

  if (dataSize < WS_MAX_CHUNK_SIZE) {
    sendFragment(conn, data, (uint8_t)frameType, true);
  } else {
    // First: fin = false, frameType = frameType
    // Following: fin = false, frameType = CONTINUATION_FRAME
    // Last: fin = true, frameType = CONTINUATION_FRAME
    size_t nChunks = dataSize / WS_MAX_CHUNK_SIZE;
    for (unsigned i = 0; i < nChunks; i++) {
      uint8_t chunkFrametype = uint8_t((i == 0) ? frameType : FrameType::CONTINUATION_FRAME);
      bool fin               = (i < (nChunks - 1)) ? false : true;
      size_t len             = (i < (nChunks - 1)) ? WS_MAX_CHUNK_SIZE : std::string::npos;
      auto const chunk       = data.substr(i * WS_MAX_CHUNK_SIZE, len);
      sendFragment(conn, chunk, chunkFrametype, fin);
    }
  }
}

} // namespace response
} // namespace websocket
} // namespace eventhub
