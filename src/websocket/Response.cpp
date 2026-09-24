#include <memory>
#include <netinet/in.h>
#include <string.h>
#include <string>

#include "Common.hpp"
#include "websocket/Response.hpp"
#include "websocket/Types.hpp"

namespace eventhub {
namespace websocket {
void Response::_appendFragment(std::string& output, std::string_view fragment,
                               std::uint8_t frameType, bool final) {
  char header[10];
  std::size_t headerSize   = 0;
  std::size_t fragmentSize = fragment.size();

  header[0] = final << 7;
  header[0] = header[0] | (0xF & frameType);
  header[1] = 0; // Server frames aren't masked.

  if (fragmentSize < 126) {
    header[1]  = static_cast<char>(fragmentSize);
    headerSize = 2;
  } else if (fragmentSize <= 0xFFFF) {
    header[1]                = 126;
    const std::uint16_t size = htons(static_cast<std::uint16_t>(fragmentSize));
    memcpy(header + 2, &size, sizeof(size));
    headerSize = 4;
  } else {
    header[1]       = 127;
    const auto size = static_cast<std::uint64_t>(fragmentSize);
    for (unsigned byte = 0; byte < 8; ++byte) {
      header[2 + byte] = static_cast<char>((size >> (56 - byte * 8)) & 0xff);
    }
    headerSize = 10;
  }

  output.append(header, headerSize);
  output.append(fragment.data(), fragment.size());
}

bool Response::sendData(ConnectionPtr connection, const std::string& data, FrameType frameType) {
  std::string output;
  if (data.empty()) {
    _appendFragment(output, {}, static_cast<std::uint8_t>(frameType), true);
    return connection->write(output);
  }

  std::size_t offset = 0;
  bool first         = true;
  while (offset < data.size()) {
    const auto size  = std::min(WS_MAX_CHUNK_SIZE, data.size() - offset);
    const bool final = offset + size == data.size();
    const auto type  = first ? frameType : FrameType::CONTINUATION_FRAME;
    _appendFragment(output, std::string_view(data).substr(offset, size),
                    static_cast<std::uint8_t>(type), final);
    first = false;
    offset += size;
  }
  return connection->write(output);
}

} // namespace websocket
} // namespace eventhub
