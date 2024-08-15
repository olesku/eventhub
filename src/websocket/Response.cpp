#include <string.h>
#include <netinet/in.h>
#include <string>
#include <memory>

#include "websocket/Response.hpp"
#include "Common.hpp"
#include "websocket/Types.hpp"

namespace eventhub {
namespace websocket {
void Response::_sendFragment(ConnectionPtr conn, const std::string& fragment, uint8_t frameType, bool fin) {
  std::string sndBuf;
  char header[8];
  std::size_t headerSize   = 0;
  std::size_t fragmentSize = fragment.size();

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

void Response::sendData(ConnectionPtr conn, const std::string& data, FrameType frameType) {
  std::size_t dataSize = data.size();

  if (dataSize < WS_MAX_CHUNK_SIZE) {
    _sendFragment(conn, data, (uint8_t)frameType, true);
  } else {
    // First: fin = false, frameType = frameType
    // Following: fin = false, frameType = CONTINUATION_FRAME
    // Last: fin = true, frameType = CONTINUATION_FRAME
    std::size_t nChunks = dataSize / WS_MAX_CHUNK_SIZE;
    for (unsigned i = 0; i < nChunks; i++) {
      uint8_t chunkFrametype = uint8_t((i == 0) ? frameType : FrameType::CONTINUATION_FRAME);
      bool fin               = (i < (nChunks - 1)) ? false : true;
      std::size_t len             = (i < (nChunks - 1)) ? WS_MAX_CHUNK_SIZE : std::string::npos;
      auto const chunk       = data.substr(i * WS_MAX_CHUNK_SIZE, len);
      _sendFragment(conn, chunk, chunkFrametype, fin);
    }
  }
}

} // namespace websocket
} // namespace eventhub
